// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/odbc/bq_driver/internal/odbc_transactions.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_type_utils.h"
#include "google/cloud/odbc/testing/bq_driver_utils/handles.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_testing_bq_driver_utils::CreateConnectionHandle;

TEST(BeginTransactionIfNeeded, Success_TransactionIsStarted) {
  ConnectionHandle conn_handle = CreateConnectionHandle(true);
  conn_handle.SetTransactionActive(true);
  conn_handle.SetAttribute(SQL_ATTR_AUTOCOMMIT,
                           ToSqlPointer(SQL_AUTOCOMMIT_OFF), 0);

  StatusRecord status = BeginTransactionIfNeeded(conn_handle);

  EXPECT_TRUE(status.ok());
}

TEST(BeginTransactionIfNeeded, Success_AutocommitIsOn) {
  ConnectionHandle conn_handle = CreateConnectionHandle(true);
  conn_handle.SetAttribute(SQL_ATTR_AUTOCOMMIT, ToSqlPointer(SQL_AUTOCOMMIT_ON),
                           0);

  StatusRecord status = BeginTransactionIfNeeded(conn_handle);

  EXPECT_TRUE(status.ok());
}

}  // namespace google::cloud::odbc_bq_driver_internal
