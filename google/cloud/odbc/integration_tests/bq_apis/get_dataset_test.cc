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

#include "google/cloud/odbc/bq_client_interface/odbc_authentication.h"
#include "google/cloud/odbc/bq_client_interface/odbc_bq_client.h"
#include "google/cloud/odbc/testing/client_library_utils/authentication.h"
#include "google/cloud/odbc/testing/client_library_utils/util_constants.h"
#include "google/cloud/odbc/testing/utils/env_vars.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include "google/cloud/bigquery/v2/minimal/internal/dataset_client.h"
#include <gmock/gmock.h>

namespace google::cloud::odbc_integration_tests_apis {

using bigquery_v2_minimal_internal::Dataset;
using bigquery_v2_minimal_internal::DatasetClient;
using bigquery_v2_minimal_internal::GetDatasetRequest;
using bigquery_v2_minimal_internal::MakeDatasetConnection;
using google::cloud::odbc_bigquery_client_interface::Oauth;
using ::google::cloud::odbc_bigquery_client_interface::OauthMechanism;
using google::cloud::odbc_bigquery_client_interface::ODBCBQClient;
using google::cloud::odbc_internal::StatusRecordOr;
using google::cloud::odbc_testing_client_library_utils::
    CreateApplicationDefaultAuthentication;
using google::cloud::odbc_testing_client_library_utils::
    CreateExternalAuthenticationBYOIDWorkforce;
using google::cloud::odbc_testing_client_library_utils::
    CreateExternalAuthenticationBYOIDWorkload;
using google::cloud::odbc_testing_client_library_utils::
    CreateExternalAuthenticationJSONFile;
using google::cloud::odbc_testing_client_library_utils::
    CreateExternalUserOauthBYOIDWorkforce;
using google::cloud::odbc_testing_client_library_utils::
    CreateExternalUserOauthBYOIDWorkload;
using google::cloud::odbc_testing_client_library_utils::
    CreateNoAccessAccountAuthentication;
using google::cloud::odbc_testing_client_library_utils::
    CreateServiceAccountAuthentication;
using google::cloud::odbc_testing_client_library_utils::
    CreateServiceAccountAuthWithClientIdAuthentication;
using google::cloud::odbc_testing_client_library_utils::
    CreateUserAccountAuthentication;
using google::cloud::odbc_testing_client_library_utils::
    kNameForNonExistingProject;
using google::cloud::odbc_testing_utils::GetRequiredEnvVar;
using google::cloud::odbc_testing_utils::StatusIs;
using ::testing::HasSubstr;

#ifdef EXTERNAL_ACCOUNT_AUTH

TEST(GetDataset, ExternalAccountAuth_JSONFile) {
  StatusOr<Options> options = CreateExternalAuthenticationJSONFile();
  ASSERT_STATUS_OK(options);
  auto dataset_client =
      DatasetClient(MakeDatasetConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string path_to_file_with_credentials =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_EXTERNAL_ACCOUNT_AUTH_KEY");

  // Retrieving dataset via ODBC BQ Client
  Oauth oauth;
  oauth.auth_mechanism = OauthMechanism::kExternalUser;
  oauth.credentials_file_path = path_to_file_with_credentials;
  auto odbc_bq_client = ODBCBQClient::CreateBQClient(oauth);
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  StatusRecordOr<Dataset> dataset_response =
      (*odbc_bq_client)
          ->GetDataset(project_id, dataset_id, std::move(*options));
  ASSERT_STATUS_RECORD_OK(dataset_response);
  EXPECT_EQ(dataset_id, (*dataset_response).dataset_reference.dataset_id);
}

TEST(GetDataset, ExternalAccountAuth_BYOID_Workload) {
  StatusOr<Options> options = CreateExternalAuthenticationBYOIDWorkload();
  ASSERT_STATUS_OK(options);
  auto dataset_client =
      DatasetClient(MakeDatasetConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");

  // Retrieving dataset via ODBC BQ Client
  Oauth oauth = CreateExternalUserOauthBYOIDWorkload();
  auto odbc_bq_client = ODBCBQClient::CreateBQClient(oauth);
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  StatusRecordOr<Dataset> dataset_response =
      (*odbc_bq_client)
          ->GetDataset(project_id, dataset_id, std::move(*options));
  ASSERT_STATUS_RECORD_OK(dataset_response);
  EXPECT_EQ(dataset_id, (*dataset_response).dataset_reference.dataset_id);
}

TEST(GetDataset, ExternalAccountAuth_BYOID_Workforce) {
  StatusOr<Options> options = CreateExternalAuthenticationBYOIDWorkforce();
  ASSERT_STATUS_OK(options);
  auto dataset_client =
      DatasetClient(MakeDatasetConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");

  // Retrieving dataset via ODBC BQ Client
  Oauth oauth = CreateExternalUserOauthBYOIDWorkforce();
  auto odbc_bq_client = ODBCBQClient::CreateBQClient(oauth);
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  StatusRecordOr<Dataset> dataset_response =
      (*odbc_bq_client)
          ->GetDataset(project_id, dataset_id, std::move(*options));
  ASSERT_STATUS_RECORD_OK(dataset_response);
  EXPECT_EQ(dataset_id, (*dataset_response).dataset_reference.dataset_id);
}

#endif  // EXTERNAL_ACCOUNT_AUTH
#ifdef USER_ACCOUNT_AUTH
TEST(GetDataset, UserAccountAuth) {
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client =
      DatasetClient(MakeDatasetConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");

  GetDatasetRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id(dataset_id);

  StatusOr<Dataset> dataset = dataset_client.GetDataset(request);

  ASSERT_STATUS_OK(dataset);
}

TEST(ODBCBQClient_GetDataset, UserAccountAuth) {
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client =
      DatasetClient(MakeDatasetConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string path_to_file_with_credentials =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_USER_ACCOUNT_AUTH_KEY");

  // Retrieving dataset via ODBC BQ Client
  Oauth oauth;
  oauth.auth_mechanism = OauthMechanism::kServiceAndUserAccount;
  oauth.credentials_file_path = path_to_file_with_credentials;
  auto odbc_bq_client = ODBCBQClient::CreateBQClient(oauth);
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  StatusRecordOr<Dataset> dataset_response =
      (*odbc_bq_client)
          ->GetDataset(project_id, dataset_id, std::move(*options));
  ASSERT_STATUS_RECORD_OK(dataset_response);
  EXPECT_EQ(dataset_id, (*dataset_response).dataset_reference.dataset_id);
}

#else

TEST(GetDataset, ServiceAccountAuth) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client =
      DatasetClient(MakeDatasetConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");

  GetDatasetRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id(dataset_id);

  StatusOr<Dataset> dataset = dataset_client.GetDataset(request);

  ASSERT_STATUS_OK(dataset);
}

TEST(GetDataset, ApplicationDefaultCredentials) {
  StatusOr<Options> options = CreateApplicationDefaultAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client =
      DatasetClient(MakeDatasetConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");

  GetDatasetRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id(dataset_id);

  StatusOr<Dataset> dataset = dataset_client.GetDataset(request);

  ASSERT_STATUS_OK(dataset);
}

TEST(ODBCBQClient_GetDataset, ApplicationDefaultCredentials) {
  StatusOr<Options> options = CreateApplicationDefaultAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client =
      DatasetClient(MakeDatasetConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");

  // Retrieving dataset via ODBC BQ Client
  auto odbc_bq_client =
      ODBCBQClient::CreateBQClient({OauthMechanism::kApplicationDefault});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  StatusRecordOr<Dataset> dataset_response =
      (*odbc_bq_client)
          ->GetDataset(project_id, dataset_id, std::move(*options));
  ASSERT_STATUS_RECORD_OK(dataset_response);
  EXPECT_EQ(dataset_id, (*dataset_response).dataset_reference.dataset_id);
}

TEST(GetDataset, DatasetNotExist) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client =
      DatasetClient(MakeDatasetConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");

  GetDatasetRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id("Non_existing_dataset");

  StatusOr<Dataset> dataset = dataset_client.GetDataset(request);

  EXPECT_THAT(dataset, StatusIs(StatusCode::kNotFound, HasSubstr("Not found")));
}

TEST(GetDataset, ProjectNotExist) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client =
      DatasetClient(MakeDatasetConnection(std::move(*options)));
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string project_id = std::string(kNameForNonExistingProject);

  GetDatasetRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id(dataset_id);

  StatusOr<Dataset> dataset = dataset_client.GetDataset(request);

  EXPECT_THAT(dataset,
              StatusIs(StatusCode::kNotFound,
                       HasSubstr("Project " + project_id + " is not found")));
}

TEST(GetDataset, ProjectIdIsEmpty) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client =
      DatasetClient(MakeDatasetConnection(std::move(*options)));
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");

  GetDatasetRequest request;
  request.set_project_id("");
  request.set_dataset_id(dataset_id);

  StatusOr<Dataset> dataset = dataset_client.GetDataset(request);

  // BQ API error
  EXPECT_THAT(dataset, StatusIs(StatusCode::kNotFound,
                                HasSubstr("Request couldn't be served")));
}

TEST(GetDataset, DatasetIdIsEmpty) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client =
      DatasetClient(MakeDatasetConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");

  GetDatasetRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id("");

  StatusOr<Dataset> dataset = dataset_client.GetDataset(request);

  // BQ API error
  EXPECT_THAT(dataset, StatusIs(StatusCode::kInternal,
                                HasSubstr("Not a valid Json Dataset object")));
}

#endif  // USER_ACCOUNT_AUTH

}  // namespace google::cloud::odbc_integration_tests_apis
