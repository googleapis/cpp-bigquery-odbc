#!/bin/bash
#
# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This bash library sets env variables for the paths of our secret files

# Make our include guard clean against set -o nounset.
test -n "${CI_CLOUDBUILD_BUILDS_LIB_SECRETS_SH__:-}" || declare -i CI_CLOUDBUILD_BUILDS_LIB_SECRETS_SH__=0
if ((CI_CLOUDBUILD_BUILDS_LIB_SECRETS_SH__++ != 0)); then
  return 0
fi # include guard

# Adds environment variables that need to reference a specific service
# account key file. The key files are copied from a GCP Secret Manager and stored on
# the local machine. See the `rotate-keys.sh` script for details about how
# these keys are rotated.
# readonly KEY_DIR="/dev/odbc-auth"
readonly KEY_DIR="/tmp/odbc-auth"
mkdir "${KEY_DIR}"
gcloud secrets versions access latest --secret=user-account-auth-keys --out-file="${KEY_DIR}/user_account_auth_keys.json"
gcloud secrets versions access latest --secret=service-account-auth-keys --out-file="${KEY_DIR}/service_account_auth_keys.json"
gcloud secrets versions access latest --secret=client-id-auth-keys --out-file="${KEY_DIR}/client_id_auth_keys.json"
gcloud secrets versions access latest --secret=wrong-account-auth-keys --out-file="${KEY_DIR}/wrong_account_auth_keys.json"
gcloud secrets versions access latest --secret=no-access-account-auth-keys --out-file="${KEY_DIR}/no_access_account_auth_keys.json"

echo "KEY FILE:::: "
cat ${KEY_DIR}/service_account_auth_keys.json

export CPP_BIGQUERY_ODBC_TEST_USER_ACCOUNT_AUTH_KEY=${KEY_DIR}/user_account_auth_keys.json
export CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY=${KEY_DIR}/service_account_auth_keys.json
export CPP_BIGQUERY_ODBC_TEST_CLIENT_ID_AUTH_KEY=${KEY_DIR}/client_id_auth_keys.json
export CPP_BIGQUERY_ODBC_TEST_WRONG_AUTH_KEY=${KEY_DIR}/wrong_account_auth_keys.json
export CPP_BIGQUERY_ODBC_TEST_NO_ACCESS_ACCOUNT_AUTH_KEY=${KEY_DIR}/no_access_account_auth_keys.json

function secrets::bazel_args() {
  declare -a args
  # Add auth keys
  args+=(
    "--test_env=CPP_BIGQUERY_ODBC_TEST_USER_ACCOUNT_AUTH_KEY=${CPP_BIGQUERY_ODBC_TEST_USER_ACCOUNT_AUTH_KEY}"
    "--test_env=CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY=${CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY}"
    "--test_env=CPP_BIGQUERY_ODBC_TEST_CLIENT_ID_AUTH_KEY=${CPP_BIGQUERY_ODBC_TEST_CLIENT_ID_AUTH_KEY}"
    "--test_env=CPP_BIGQUERY_ODBC_TEST_WRONG_AUTH_KEY=${CPP_BIGQUERY_ODBC_TEST_WRONG_AUTH_KEY}"
    "--test_env=CPP_BIGQUERY_ODBC_TEST_NO_ACCESS_ACCOUNT_AUTH_KEY=${CPP_BIGQUERY_ODBC_TEST_NO_ACCESS_ACCOUNT_AUTH_KEY}"
  )
  printf "%s\n" "${args[@]}"
}
