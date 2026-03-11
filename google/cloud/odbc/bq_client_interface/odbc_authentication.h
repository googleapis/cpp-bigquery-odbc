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
#include <nlohmann/json.hpp>

namespace google::cloud::odbc_bigquery_client_interface {

// BYOID: Subject token types
std::string const kSubTokenTypeJWT = "urn:ietf:params:oauth:token-type:jwt";
std::string const kSubTokenTypeIdToken =
    "urn:ietf:params:oauth:token-type:id-token";
std::string const kSubTokenTypeSaml2 = "urn:ietf:params:oauth:token-type:saml2";
std::string const kSubTokenTypeAws4 =
    "urn:ietf:params:aws:token-type:aws4_request";

// Default BYOID properties.
std::string const kSubTokenTypeDefault = kSubTokenTypeIdToken;
std::string const kDefaultTokenUrl = "https://sts.googleapis.com/v1/token";

// NOTE: This should always specify the integral values with the type names
//  because the driver layer is tightly coupled to the integer values.
enum class OauthMechanism {
  kServiceAccount = 0,
  kUserAccount = 1,
  kApplicationDefault = 3,
  kExternalUser = 4,
};

struct SslCredentials {
  std::string pem_root_certs;
#ifdef _WIN32
  bool use_system_trust_store = false;
#endif
};

struct ProxyOptions {
  std::string hostname;
  std::string port;
  std::string username;
  std::string password;
};

struct TPC {
  bool enable_tpc;
  std::string universe_domain;
};

struct Oauth {
  OauthMechanism auth_mechanism;
  std::string credentials_file_path;
  /////////////////////////////////////////////////////////////////
  // Optional BYOID Properties needed for external authentication.
  /////////////////////////////////////////////////////////////////
  // The audience which the token is intended for
  std::string byoid_aud_url;
  // A json object describing the file location of the subject token, or the URI
  // to request it.
  std::string byoid_creds_src;
  // The project number associated with the workforce pool. Populated only for
  // workforce authentication.
  std::string byoid_pool_user_project;
  // The subject token type (JWT/SAML/Id token..). Defaults to
  // urn:ietf:params:oauth:tokentype:id_token.
  std::string byoid_subj_token_type;
  // The URI used to generate authentication tokens. Defaults to
  // https://sts.googleapis.com/v1/token.
  std::string byoid_token_url;
  SslCredentials ssl_credentials;
  ProxyOptions proxy_options;
  std::string kms_key_name;
  std::string psc;
  TPC tpc;
};

// Returns true if all required BYOID properties are set.
inline bool IsBYOIDPropsSet(Oauth const& oauth) {
  return (!oauth.byoid_aud_url.empty() && !oauth.byoid_creds_src.empty() &&
          !oauth.byoid_subj_token_type.empty());
}

/// Creates an object of UnifiedCredentials depending on the input arguments.
odbc_internal::StatusRecordOr<std::shared_ptr<Credentials>> CreateCredentials(
    Oauth const& oauth,
    ::google::cloud::Options const& options = ::google::cloud::Options{});

/// Creates OAuth2 access_token
odbc_internal::StatusRecordOr<AccessToken> GetOAuth2Token(
    std::shared_ptr<::google::cloud::oauth2::AccessTokenGenerator> const&
        generator);

///////////////////////////////////////////////
// Helper functions for external authentication
///////////////////////////////////////////////
odbc_internal::StatusRecordOr<nlohmann::json> CreateJsonCredsObject(
    std::string const& byoid_aud_url, std::string const& byoid_creds_source,
    std::string const& byoid_pool_user_project,
    std::string const& byoid_sub_token_type,
    std::string const& byoid_token_url);

}  // namespace google::cloud::odbc_bigquery_client_interface

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_CLIENT_INTERFACE_ODBC_AUTHENTICATION_H
