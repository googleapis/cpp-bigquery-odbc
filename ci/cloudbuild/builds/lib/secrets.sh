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
readonly KEY_DIR="${ODBC_AUTH_KEY_DIR:-/dev/odbc-auth}"
mkdir -p "${KEY_DIR}"

function secrets::fetch_secret() {
  local secret="$1"
  local out_file="$2"
  local project="${GOOGLE_CLOUD_PROJECT:-${PROJECT_ID:-}}"
  if [[ -n "${project}" ]]; then
    gcloud secrets versions access latest --project="${project}" --secret="${secret}" --out-file="${out_file}"
  else
    gcloud secrets versions access latest --secret="${secret}" --out-file="${out_file}"
  fi
}

# TODO(b/383592620): Update 'external-account-auth-keys' secret value
# to the latest generated credentials especially the "CHANGE_ME" keys.
secrets::fetch_secret external-account-auth-keys "${KEY_DIR}/external_account_auth_keys.json"
secrets::fetch_secret user-account-auth-keys "${KEY_DIR}/user_account_auth_keys.json"
secrets::fetch_secret service-account-auth-keys "${KEY_DIR}/service_account_auth_keys.json"
# TODO(b/333011414) Enable it or remove
# secrets::fetch_secret client-id-auth-keys "${KEY_DIR}/client_id_auth_keys.json"
secrets::fetch_secret wrong-account-auth-keys "${KEY_DIR}/wrong_account_auth_keys.json"
secrets::fetch_secret no-access-account-auth-keys "${KEY_DIR}/no_access_account_auth_keys.json"
secrets::fetch_secret external-auth-token-script "${KEY_DIR}/external_auth_token.sh"
chmod +x "${KEY_DIR}/external_auth_token.sh"
if [[ "${GOOGLE_CLOUD_BUILD:-}" != "yes" ]]; then
  # Outside Cloud Build (e.g. Kokoro or local Docker on GCE),
  # metadata.google.internal may return a host VM token without the
  # iamcredentials scope. Force external_auth_token.sh to fall back to
  # `gcloud auth print-access-token` using the test service account key.
  sed -i -E 's|TOKEN=\$\(curl .*|TOKEN=""|; s|http://metadata.google.internal|http://127.0.0.1:1|g' \
    "${KEY_DIR}/external_auth_token.sh"
  CLOUDSDK_AUTH_CREDENTIAL_FILE_OVERRIDE="${KEY_DIR}/service_account_auth_keys.json" \
    "${KEY_DIR}/external_auth_token.sh" "${KEY_DIR}/tkn.txt"
else
  "${KEY_DIR}/external_auth_token.sh" "${KEY_DIR}/tkn.txt"
fi
if [[ ! -s "${KEY_DIR}/tkn.txt" ]]; then
  echo "Error: ${KEY_DIR}/tkn.txt is empty or does not exist!" >&2
  exit 1
fi
sed -i "s|<some-path>|${KEY_DIR}|g" "${KEY_DIR}/external_account_auth_keys.json"

export CPP_BIGQUERY_ODBC_TEST_USER_ACCOUNT_AUTH_KEY=${KEY_DIR}/user_account_auth_keys.json
export CPP_BIGQUERY_ODBC_TEST_EXTERNAL_ACCOUNT_AUTH_KEY=${KEY_DIR}/external_account_auth_keys.json
export CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY=${KEY_DIR}/service_account_auth_keys.json
export CPP_BIGQUERY_ODBC_TEST_CLIENT_ID_AUTH_KEY=${KEY_DIR}/client_id_auth_keys.json
export CPP_BIGQUERY_ODBC_TEST_WRONG_AUTH_KEY=${KEY_DIR}/wrong_account_auth_keys.json
export CPP_BIGQUERY_ODBC_TEST_NO_ACCESS_ACCOUNT_AUTH_KEY=${KEY_DIR}/no_access_account_auth_keys.json
export GOOGLE_APPLICATION_CREDENTIALS=${KEY_DIR}/service_account_auth_keys.json

function secrets::bazel_args() {
  declare -a args
  # Add auth keys
  args+=(
    "--test_env=CPP_BIGQUERY_ODBC_TEST_EXTERNAL_ACCOUNT_AUTH_KEY=${CPP_BIGQUERY_ODBC_TEST_EXTERNAL_ACCOUNT_AUTH_KEY}"
    "--test_env=CPP_BIGQUERY_ODBC_TEST_USER_ACCOUNT_AUTH_KEY=${CPP_BIGQUERY_ODBC_TEST_USER_ACCOUNT_AUTH_KEY}"
    "--test_env=CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY=${CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY}"
    "--test_env=GOOGLE_APPLICATION_CREDENTIALS=${GOOGLE_APPLICATION_CREDENTIALS}"
    "--test_env=CPP_BIGQUERY_ODBC_TEST_CLIENT_ID_AUTH_KEY=${CPP_BIGQUERY_ODBC_TEST_CLIENT_ID_AUTH_KEY}"
    "--test_env=CPP_BIGQUERY_ODBC_TEST_WRONG_AUTH_KEY=${CPP_BIGQUERY_ODBC_TEST_WRONG_AUTH_KEY}"
    "--test_env=CPP_BIGQUERY_ODBC_TEST_NO_ACCESS_ACCOUNT_AUTH_KEY=${CPP_BIGQUERY_ODBC_TEST_NO_ACCESS_ACCOUNT_AUTH_KEY}"
  )
  printf "%s\n" "${args[@]}"
}
