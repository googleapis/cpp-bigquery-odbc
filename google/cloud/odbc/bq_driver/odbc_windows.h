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
#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_WINDOWS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_WINDOWS_H

#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/internal/utils.h"

namespace google::cloud::odbc_bq_driver {

bool ConfigDSNInternal(HWND hwnd_parent, WORD f_request, LPCSTR lpsz_driver,
                       LPCSTR lpsz_attributes);

std::string ConvertLogLevel(std::string log_level);

std::string const dsn_key = "DSN";
std::string const key_file_path_key = "KeyFilePath";
std::string const oauth_mechanism_key = "OAuthMechanism";
std::string const catalog_key = "Catalog";
std::string const dataset_key = "DefaultDataset";
std::string const encrypt_data_key = "EncryptData";
std::string const trusted_certs_key = "TrustedCerts";
std::string const min_tls_key = "Min_TLS";
std::string const description_key = "Description";
std::string const sql_dialect_key = "SQLDialect";
std::string const large_results_dataset_key = "LargeResultsDatasetId";
std::string const encryption_key = "KMSKeyName";
std::string const rows_per_block_key = "RowsFetchedPerBlock";
std::string const default_string_length_key = "DefaultStringColumnLength";
std::string const temp_expiration_key = "LargeResultsTempTableExpirationTime";
std::string const session_location_key = "SessionLocation";
std::string const additional_projects_key = "AdditionalProjects";
std::string const query_properties_key = "QueryProperties";
std::string const max_threads_key = "MaxThreads";
std::string const activation_threshold_key = "HTAPI_ActivationThreshold";
std::string const use_wchar_key = "UseWVarChar";
std::string const enable_session_key = "EnableSession";
std::string const htapi_activation_threshold_check_key =
    "AllowHtapiForLargeResults";
std::string const allow_large_results_key = "AllowLargeResults";
std::string const encryption_type = "EncryptionType";
std::string const use_default_large_results_dataset_key =
    "UseDefaultLargeResultsDataset";
std::string const proxy_check_key = "ProxyEnable";
std::string const proxy_host_key = "ProxyHost";
std::string const proxy_port_key = "ProxyPort";
std::string const proxy_username_key = "ProxyUid";
std::string const proxy_pwd_key = "ProxyPwd";
std::string const proxy_pwd_enc_key = "ProxyPwd_Enc";

}  // namespace google::cloud::odbc_bq_driver
#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_WINDOWS_H
