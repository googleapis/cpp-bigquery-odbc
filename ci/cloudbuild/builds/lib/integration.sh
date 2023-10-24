#!/bin/bash
#
# Copyright 2023 Google LLC
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

# This bash library has various helper functions for our integration tests.

# Make our include guard clean against set -o nounset.
test -n "${CI_CLOUDBUILD_BUILDS_LIB_INTEGRATION_SH__:-}" || declare -i CI_CLOUDBUILD_BUILDS_LIB_INTEGRATION_SH__=0
if ((CI_CLOUDBUILD_BUILDS_LIB_INTEGRATION_SH__++ != 0)); then
  return 0
fi # include guard

CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT=bigquery-devtools-drivers
CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET=INTEGRATION_TESTS
CPP_BIGQUERY_ODBC_TEST_TABLE_NAME=Test_Table

# Outputs a list of Bazel arguments that should be used when running
# integration tests. These do not include the common `bazel::common_args`.
#
# Example usage:
#
#   mapfile -t args < <(bazel::common_args)
#   mapfile -t integration_args < <(integration::bazel_args)
#   bazel test "${args[@]}" "${integration_args[@]}"
#
function integration::bazel_args() {
  declare -a args

  # Integration tests are inherently flaky. Make up to three attempts to get the
  # test passing.
  args+=(--flaky_test_attempts=3)

  args+=(
    "--test_env=CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT=${CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT}"
    "--test_env=CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET=${CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET}"
    "--test_env=CPP_BIGQUERY_ODBC_TEST_TABLE_NAME=${CPP_BIGQUERY_ODBC_TEST_TABLE_NAME}"
    # It's required for running 'blaze test' in docker and locally for default application auth
    "--test_env=HOME"
  )

  # Adds environment variables that need to reference a specific service
  # account key file. The key files are copied from a GCP Secret Manager and stored on
  # the local machine. See the `rotate-keys.sh` script for details about how
  # these keys are rotated.
  readonly KEY_DIR="/dev/odbc-auth"
  mkdir "${KEY_DIR}"
  gcloud secrets versions access latest --secret=user-account-auth-keys --out-file="${KEY_DIR}/user_account_auth_keys.json"
  gcloud secrets versions access latest --secret=client-id-auth-keys --out-file="${KEY_DIR}/client_id_auth_keys.json"
  gcloud secrets versions access latest --secret=wrong-account-auth-keys --out-file="${KEY_DIR}/wrong_account_auth_keys.json"
  args+=(
    "--test_env=CPP_BIGQUERY_ODBC_TEST_USER_ACCOUNT_ACCOUNT_KEY=${KEY_DIR}/user_account_auth_keys.json"
    "--test_env=CPP_BIGQUERY_ODBC_TEST_CLIENT_ID_ACCOUNT_KEY=${KEY_DIR}/client_id_auth_keys.json"
    "--test_env=CPP_BIGQUERY_ODBC_TEST_WRONG_AUTH_KEY=${KEY_DIR}/wrong_account_auth_keys.json"
  )
  printf "%s\n" "${args[@]}"
}
