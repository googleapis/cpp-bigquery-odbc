// Copyright 2026 Google LLC
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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_CLIENT_INTERFACE_MTLS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_CLIENT_INTERFACE_MTLS_H

#include "google/cloud/odbc/bq_client_interface/odbc_authentication.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/oauth2/access_token_generator.h"
#include "google/cloud/ssl_certificate.h"
#include "absl/types/optional.h"
#include <grpcpp/security/credentials.h>
#include <memory>
#include <string>
#include <vector>

namespace google::cloud::odbc_bigquery_client_interface {

inline char const kUseClientCertificateEnvVar[] =
    "GOOGLE_API_USE_CLIENT_CERTIFICATE";
inline char const kUseMtlsEndpointEnvVar[] = "GOOGLE_API_USE_MTLS_ENDPOINT";

inline char const kBigQueryMtlsEndpoint[] = "bigquery.mtls.googleapis.com";
inline char const kBigQueryStorageMtlsEndpoint[] =
    "bigquerystorage.mtls.googleapis.com";
inline char const kResourceManagerMtlsEndpoint[] =
    "cloudresourcemanager.mtls.googleapis.com";
inline char const kServiceUsageMtlsEndpoint[] =
    "serviceusage.mtls.googleapis.com";

inline char const kSecureConnectMetadataPath[] =
    ".secureConnect/context_aware_metadata.json";

struct MtlsConfig {
  // The client certificate to present during the TLS handshake, if any.
  absl::optional<::google::cloud::experimental::SslCertificate>
      client_certificate;
  bool use_mtls_endpoint = false;
};

// The certificate chain and private key found in a PEM bundle.
struct PemBundle {
  std::string certificates;
  std::string private_key;
  bool encrypted_private_key = false;
};

// Splits a PEM bundle into certificate chain and private key.
PemBundle SplitPemBundle(std::string const& contents);

// Parses the SecureConnect metadata file to extract cert_provider_command.
odbc_internal::StatusRecordOr<std::vector<std::string>>
ParseCertProviderCommand(std::string const& metadata_json);

// Executes command argv (without shell) and returns its stdout.
odbc_internal::StatusRecordOr<std::string> RunCommandCaptureStdout(
    std::vector<std::string> const& argv);

// Loads client certificate and private key from PEM file(s).
odbc_internal::StatusRecordOr<::google::cloud::experimental::SslCertificate>
LoadClientCertificateFromFiles(std::string const& certificate_path,
                               std::string const& private_key_path);

// Loads the default device certificate via Endpoint Verification metadata.
odbc_internal::StatusRecordOr<
    absl::optional<::google::cloud::experimental::SslCertificate>>
LoadDefaultClientCertificate();

// Resolves mTLS configuration per AIP-4114.
odbc_internal::StatusRecordOr<MtlsConfig> ResolveMtlsConfig(
    MtlsOptions const& options, bool is_default_universe);

// Creates gRPC channel credentials carrying client certificate and OAuth
// tokens.
std::shared_ptr<grpc::ChannelCredentials> MakeMtlsGrpcChannelCredentials(
    ::google::cloud::experimental::SslCertificate const& certificate,
    std::string const& ca_roots_file_path,
    std::shared_ptr<::google::cloud::oauth2::AccessTokenGenerator> generator);

}  // namespace google::cloud::odbc_bigquery_client_interface

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_CLIENT_INTERFACE_MTLS_H
