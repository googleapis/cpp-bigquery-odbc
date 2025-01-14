// Copyright 2023 Google LLC
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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_CLIENT_INTERFACE_ODBC_AUTHENTICATION_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_CLIENT_INTERFACE_ODBC_AUTHENTICATION_H

#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/credentials.h"
#include "google/cloud/oauth2/access_token_generator.h"
#include "google/cloud/status_or.h"
#include "absl/strings/string_view.h"

namespace google::cloud::odbc_bigquery_client_interface {

// NOTE: This should always specify the integral values with the type names
//  because the driver layer is tightly coupled to the integer values.
enum class OauthMechanism {
  kServiceAndUserAccount = 0,
  kApplicationDefault = 3,
  kExternalUser = 4,
};

struct Oauth {
  OauthMechanism auth_mechanism;
  std::string credentials_file_path;
};

/// Creates an object of UnifiedCredentials depending on the input arguments.
odbc_internal::StatusRecordOr<std::shared_ptr<Credentials>> CreateCredentials(
    Oauth const& oauth);

/// Creates OAuth2 access_token
odbc_internal::StatusRecordOr<AccessToken> GetOAuth2Token(
    std::shared_ptr<::google::cloud::oauth2::AccessTokenGenerator> const&
        generator);

}  // namespace google::cloud::odbc_bigquery_client_interface

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_CLIENT_INTERFACE_ODBC_AUTHENTICATION_H
