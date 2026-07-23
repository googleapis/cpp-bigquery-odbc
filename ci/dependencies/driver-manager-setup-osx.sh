#!/usr/bin/env bash
#
# Copyright 2025 Google LLC
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

set -euo pipefail

# Make our include guard clean against set -o nounset.
test -n "${CI_DEPENDENCIES_GOOGLE_DRIVER_MANAGER_SETUP_OSX_SH__:-}" || declare -i CI_DEPENDENCIES_GOOGLE_DRIVER_MANAGER_SETUP_OSX_SH__=0
if ((CI_DEPENDENCIES_GOOGLE_DRIVER_MANAGER_SETUP_OSX_SH__++ != 0)); then
  return 0
fi # include guard

CPP_GOOGLE_BIGQUERY_ODBC_DRIVER_MANAGER_SETUP_OSX_CURR_DIR="$(pwd)"
export CPP_GOOGLE_BIGQUERY_ODBC_DRIVER_MANAGER_SETUP_OSX_CURR_DIR
export GCS_BUCKET=bq-dev-tools-testing-drivers

# Check gcloud is installed.
echo "Verifying google cloud SDK is installed using GCS Bucket: "${GCS_BUCKET}
if [ "$(gsutil ls gs://${GCS_BUCKET}/odbc | grep -c odbc-driver.zip)" -eq 0 ]; then
  echo 'ODBC driver not found for download: exiting...'
  exit 1
fi

# Configure connection credentials for the driver.
echo 'Configuring Connection Credentials...'
mkdir -p /Users/runner/work/connection
mkdir -p /Users/runner/work/connection/odbc-driver
cd /Users/runner/work/connection/odbc-driver

# Fetch standard keys
gcloud secrets versions access latest --secret=service-account-auth-keys --out-file="/Users/runner/work/connection/key.json"

# Fetch external keys and generate token
gcloud secrets versions access latest --secret=external-account-auth-keys --out-file="/Users/runner/work/connection/external_account_auth_keys.json"
gcloud secrets versions access latest --secret=external-auth-token-script --out-file="/Users/runner/work/connection/external_auth_token.sh"
chmod +x /Users/runner/work/connection/external_auth_token.sh
/Users/runner/work/connection/external_auth_token.sh /Users/runner/work/connection/tkn.txt
if [ ! -s "/Users/runner/work/connection/tkn.txt" ]; then
  echo "Error: /Users/runner/work/connection/tkn.txt is empty or does not exist!" >&2
  exit 1
fi

# shellcheck disable=SC2016
jq '.credential_source.file = "/Users/runner/work/connection/tkn.txt"' /Users/runner/work/connection/external_account_auth_keys.json >/Users/runner/work/connection/external_account_auth_keys.json.tmp
mv /Users/runner/work/connection/external_account_auth_keys.json.tmp /Users/runner/work/connection/external_account_auth_keys.json

# Persist environment variables so macos-cmake.sh and tests can access them
if [[ -n "${GITHUB_ENV:-}" ]]; then
  echo "CPP_BIGQUERY_ODBC_TEST_EXTERNAL_ACCOUNT_AUTH_KEY=/Users/runner/work/connection/external_account_auth_keys.json" >>"$GITHUB_ENV"
fi

echo 'Verifying Connection Keys File Size...'
file_size=$(stat -f '%z' /Users/runner/work/connection/key.json)
if [[ $file_size =~ ^[0-9]+$ ]] && [ "$file_size" -lt 100 ]; then
  echo 'Invalid connection keys: exiting...'
  exit 1
fi

cd "$CPP_GOOGLE_BIGQUERY_ODBC_DRIVER_MANAGER_SETUP_OSX_CURR_DIR"

source "$(dirname "$0")/../lib/init.sh"
source module ci/gha/builds/lib/cmake.sh

mapfile -t args < <(cmake::common_args)
args+=(
  -DODBC_UNIT_TESTING=OFF
  -DODBC_INTEGRATION_TESTING=ON
  -DBQ_DRIVER_INTEGRATION_TESTS=ON
  -DCLIENT_LIBRARY_INTEGRATION_TESTING=OFF
  -DCMAKE_CXX_FLAGS="-I$(brew --prefix libiodbc)/include"
  -DCMAKE_CXX_STANDARD=17
)

mapfile -t vcpkg_args < <(cmake::vcpkg_args)

io::log_h1 "Starting Build"
TIMEFORMAT="==> 🕑 CMake configuration done in %R seconds"
time {
  io::run cmake "${args[@]}" "${vcpkg_args[@]}"
}

TIMEFORMAT="==> 🕑 CMake build done in %R seconds"
time {
  io::run cmake --build cmake-out
}

echo '**** ODBC Driver Setup END****'
