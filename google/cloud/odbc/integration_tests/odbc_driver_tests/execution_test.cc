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


#include <gtest/gtest.h>

#include "google/cloud/odbc/testing/odbc_utils/connection.h"

namespace google::cloud::odbc_tests {

class QueryExecutionTestSuite : public ::testing::Test {
 protected:
  void SetUp() override {

    std::cout << "Setup::: " << std::endl;
    // You can add any setup code here that needs to be done only once
    // before all tests in the suite are run.
    // For example, you can create a connection or a statement object here.
  }

  void TearDown() override {
    std::cout << "Teardown::: " << std::endl;
    // You can add any teardown code here that needs to be done only once
    // after all tests in the suite are run.
    // For example, you can close the connection or the statement object here.
  }
};

TEST_F(QueryExecutionTestSuite, SQLGetCursorName1) {
  std::cout << "SQLGetCursorName1::: " << std::endl;
}

TEST_F(QueryExecutionTestSuite, SQLGetCursorName2) {
  std::cout << "SQLGetCursorName2::: " << std::endl;
}

}  // namespace google::cloud::odbc_tests
