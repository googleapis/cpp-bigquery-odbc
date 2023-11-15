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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_INTEGRATION_TESTS_TESTING_UTIL_AUTHENTICATION_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_INTEGRATION_TESTS_TESTING_UTIL_AUTHENTICATION_H

#include "google/cloud/options.h"
#include "google/cloud/status_or.h"

namespace google {
namespace cloud {
namespace odbc_testing_util_internal {

// Creates Options object which has credentials for User Account Authentication.
// Updates GOOGLE_APPLICATION_CREDENTIALS env var.
StatusOr<Options> CreateUserAccountAuthentication();

// Creates Options object which has credentials for Service Account Authentication.
StatusOr<Options> CreateServiceAccountAuthentication();

// Creates Options object which has credentials for Service Account With Client ID Authentication.
// Updates GOOGLE_APPLICATION_CREDENTIALS env var.
StatusOr<Options> CreateServiceAccountAuthWithClientIdAuthentication();

// Creates WRONG Options object with the path to not existing file.
// Updates GOOGLE_APPLICATION_CREDENTIALS env var.
StatusOr<Options> CreateWrongPathToAuthFileAuthentication();

// Creates WRONG Options object has not existing credentials.
// Updates GOOGLE_APPLICATION_CREDENTIALS env var.
StatusOr<Options> CreateWrongAuthentication();

// Creates WRONG Options object of the user with 0 projects to access.
// Updates GOOGLE_APPLICATION_CREDENTIALS env var.
StatusOr<Options> CreateNoAccessAccountAuthentication();
}
}
}

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_INTEGRATION_TESTS_TESTING_UTIL_AUTHENTICATION_H
