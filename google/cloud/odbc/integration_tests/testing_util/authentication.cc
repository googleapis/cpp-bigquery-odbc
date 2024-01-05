// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gtest/gtest.h>
#include <fstream>

#include "google/cloud/options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/status_or.h"

namespace google::cloud::odbc_integration_tests_testing_util {

using google::cloud::internal::GetEnv;

StatusOr<Options> CreateUserAccountAuthentication() {
  std::string path_to_file_with_credentials = GetEnv("CPP_BIGQUERY_ODBC_TEST_USER_ACCOUNT_AUTH_KEY").value_or("");
  if (path_to_file_with_credentials.empty()) {
    return Status(StatusCode::kInvalidArgument, "CPP_BIGQUERY_ODBC_TEST_USER_ACCOUNT_AUTH_KEY environment variable is not set");
  }
  setenv("GOOGLE_APPLICATION_CREDENTIALS", path_to_file_with_credentials.c_str(), 1);
  return google::cloud::Options{}.set<google::cloud::UnifiedCredentialsOption>(
      google::cloud::MakeGoogleDefaultCredentials());
}

StatusOr<Options> CreateServiceAccountAuthentication() {
  std::string path_to_file_with_credentials = GetEnv("CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY").value_or("");
  if (path_to_file_with_credentials.empty()) {
    return Status(StatusCode::kInvalidArgument, "CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY environment variable is not set");
  }
  auto is = std::ifstream(path_to_file_with_credentials);
  is.exceptions(std::ios::badbit);  // Minimal error handling
  auto contents = std::string(std::istreambuf_iterator<char>(is.rdbuf()), {});
  return google::cloud::Options{}.set<google::cloud::UnifiedCredentialsOption>(
          google::cloud::MakeServiceAccountCredentials(contents));
}

StatusOr<Options> CreateServiceAccountAuthWithClientIdAuthentication() {
  std::string path_to_file_with_credentials = GetEnv("CPP_BIGQUERY_ODBC_TEST_CLIENT_ID_AUTH_KEY").value_or("");
  if (path_to_file_with_credentials.empty()) {
    return Status(StatusCode::kInvalidArgument, "CPP_BIGQUERY_ODBC_TEST_CLIENT_ID_AUTH_KEY environment variable is not set");
  }
  setenv("GOOGLE_APPLICATION_CREDENTIALS", path_to_file_with_credentials.c_str(), 1);
  return google::cloud::Options{}.set<google::cloud::UnifiedCredentialsOption>(
      google::cloud::MakeGoogleDefaultCredentials());
}

StatusOr<Options> CreateWrongPathToAuthFileAuthentication() {
  setenv("GOOGLE_APPLICATION_CREDENTIALS", "path-to-non-existing-file.json", 1);
  return google::cloud::Options{}.set<google::cloud::UnifiedCredentialsOption>(
      google::cloud::MakeGoogleDefaultCredentials());
}

StatusOr<Options> CreateWrongAuthentication() {
  std::string path_to_file_with_credentials = GetEnv("CPP_BIGQUERY_ODBC_TEST_WRONG_AUTH_KEY").value_or("");
  if (path_to_file_with_credentials.empty()) {
    return Status(StatusCode::kInvalidArgument, "CPP_BIGQUERY_ODBC_TEST_WRONG_AUTH_KEY environment variable is not set");
  }
  setenv("GOOGLE_APPLICATION_CREDENTIALS", path_to_file_with_credentials.c_str(), 1);
  return google::cloud::Options{}.set<google::cloud::UnifiedCredentialsOption>(
      google::cloud::MakeGoogleDefaultCredentials());
}

StatusOr<Options> CreateNoAccessAccountAuthentication() {
  std::string path_to_file_with_credentials = GetEnv("CPP_BIGQUERY_ODBC_TEST_NO_ACCESS_ACCOUNT_AUTH_KEY").value_or("");
  if (path_to_file_with_credentials.empty()) {
    return Status(StatusCode::kInvalidArgument, "CPP_BIGQUERY_ODBC_TEST_NO_ACCESS_ACCOUNT_AUTH_KEY environment variable is not set");
  }
  setenv("GOOGLE_APPLICATION_CREDENTIALS", path_to_file_with_credentials.c_str(), 1);
  return google::cloud::Options{}.set<google::cloud::UnifiedCredentialsOption>(
      google::cloud::MakeGoogleDefaultCredentials());
}

} // namespace google::cloud::odbc_integration_tests_testing_util
