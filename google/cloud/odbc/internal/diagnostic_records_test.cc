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

#include "google/cloud/odbc/internal/diagnostic_records.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_internal {

TEST(ConvertFrom, kInvalidArgument) {
  Status status(StatusCode::kInvalidArgument, "message");

  StatusRecord status_record = StatusRecord::ConvertFrom(status);

  EXPECT_EQ(SQLStates::k_42000(), status_record.sql_state);
  EXPECT_EQ("[BigQuery] message", status_record.message);
  EXPECT_EQ(400, status_record.native_error_code);
}

TEST(ConvertFrom, kUnauthenticated) {
  Status status(StatusCode::kUnauthenticated, "message");

  StatusRecord status_record = StatusRecord::ConvertFrom(status);

  EXPECT_EQ(SQLStates::k_28000(), status_record.sql_state);
  EXPECT_EQ("[BigQuery] message", status_record.message);
  EXPECT_EQ(401, status_record.native_error_code);
}

TEST(ConvertFrom, kPermissionDenied) {
  Status status(StatusCode::kPermissionDenied, "message");

  StatusRecord status_record = StatusRecord::ConvertFrom(status);

  EXPECT_EQ(SQLStates::k_42000(), status_record.sql_state);
  EXPECT_EQ("[BigQuery] message", status_record.message);
  EXPECT_EQ(403, status_record.native_error_code);
}

TEST(ConvertFrom, kNotFound) {
  Status status(StatusCode::kNotFound, "message");

  StatusRecord status_record = StatusRecord::ConvertFrom(status);

  EXPECT_EQ(SQLStates::k_HY000(), status_record.sql_state);
  EXPECT_EQ("[BigQuery] message", status_record.message);
  EXPECT_EQ(404, status_record.native_error_code);
}

TEST(ConvertFrom, kAborted) {
  Status status(StatusCode::kAborted, "message");

  StatusRecord status_record = StatusRecord::ConvertFrom(status);

  EXPECT_EQ(SQLStates::k_HY000(), status_record.sql_state);
  EXPECT_EQ("[BigQuery] message", status_record.message);
  EXPECT_EQ(409, status_record.native_error_code);
}

TEST(ConvertFrom, kInternal) {
  Status status(StatusCode::kInternal, "message");

  StatusRecord status_record = StatusRecord::ConvertFrom(status);

  EXPECT_EQ(SQLStates::k_HY000(), status_record.sql_state);
  EXPECT_EQ("[BigQuery] message", status_record.message);
  EXPECT_EQ(501, status_record.native_error_code);
}

TEST(ConvertFrom, kUnavailable) {
  Status status(StatusCode::kUnavailable, "message");

  StatusRecord status_record = StatusRecord::ConvertFrom(status);

  EXPECT_EQ(SQLStates::k_HY000(), status_record.sql_state);
  EXPECT_EQ("[BigQuery] message", status_record.message);
  EXPECT_EQ(500, status_record.native_error_code);
}

}  // namespace google::cloud::odbc_internal
