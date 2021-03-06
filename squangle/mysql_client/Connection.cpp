/*
 *  Copyright (c) 2015, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include "squangle/mysql_client/Connection.h"
#include "squangle/mysql_client/AsyncMysqlClient.h"
#include "squangle/mysql_client/AsyncConnectionPool.h"

namespace facebook {
namespace common {
namespace mysql_client {

MysqlConnectionHolder::MysqlConnectionHolder(AsyncMysqlClient* client,
                                             MYSQL* mysql,
                                             const ConnectionKey conn_key,
                                             bool already_open)
    : async_client_(client),
      mysql_(mysql),
      conn_key_(conn_key),
      connection_opened_(already_open),
      can_reuse_(true) {
  async_client_->activeConnectionAdded(&conn_key_);
  creation_time_ = std::chrono::high_resolution_clock::now();
}

MysqlConnectionHolder::MysqlConnectionHolder(
    std::unique_ptr<MysqlConnectionHolder> from_holder)
    : async_client_(from_holder->async_client_),
      conn_key_(from_holder->conn_key_),
      creation_time_(from_holder->creation_time_),
      connection_opened_(from_holder->connection_opened_),
      can_reuse_(from_holder->can_reuse_) {
  mysql_ = from_holder->stealMysql();
  async_client_->activeConnectionAdded(&conn_key_);
}

bool MysqlConnectionHolder::inTransaction() {
  return mysql_->server_status & SERVER_STATUS_IN_TRANS;
}

MysqlConnectionHolder::~MysqlConnectionHolder() {
  if (mysql_) {
    auto mysql = mysql_;
    auto client = async_client_;
    // Close our connection in the thread from which it was created.
    if (!async_client_->runInThread([mysql, client]() {
          mysql_close(mysql);
        })) {
      LOG(DFATAL) << "Mysql connection couldn't be closed: error in TEventBase";
    }
    if (connection_opened_) {
      async_client_->stats()->incrClosedConnections();
    }
  }
  async_client_->activeConnectionRemoved(&conn_key_);
}

void MysqlConnectionHolder::connectionOpened() {
  connection_opened_ = true;
  async_client_->stats()->incrOpenedConnections();
}
}
}
} // namespace facebook::common::mysql_client
