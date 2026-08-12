#!/usr/bin/env bash
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

set -euo pipefail

source "$(dirname "$0")/../../lib/init.sh"
source module ci/gha/builds/lib/macos.sh
source module ci/gha/builds/lib/cmake.sh

# Ensure directories exist
mkdir -p /Users/runner/work/connection
mkdir -p /Users/runner/work/connection/odbc-driver

# Fetch standard keys
gcloud secrets versions access latest --secret=service-account-auth-keys --out-file="/Users/runner/work/connection/key.json"
echo 'Verifying Connection Keys File Size...'
file_size=$(stat -f '%z' /Users/runner/work/connection/key.json)
if [[ $file_size =~ ^[0-9]+$ ]] && [ "$file_size" -lt 100 ]; then
  echo 'Invalid connection keys: exiting...'
  exit 1
fi

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

cp ci/gha/builds/lib/odbc_osx.ini /Users/runner/work/connection/odbc-driver/odbc.ini
cp ci/gha/builds/lib/odbcinst_osx.ini /Users/runner/work/connection/odbc-driver/odbcinst.ini
cp ci/gha/builds/lib/googlebigqueryodbc.ini /Users/runner/work/connection/googlebigqueryodbc.ini
cp ci/etc/googlebigqueryodbc_utf16.ini /Users/runner/work/connection/googlebigqueryodbc_utf16.ini
cp ci/etc/googlebigqueryodbc_utf8.ini /Users/runner/work/connection/googlebigqueryodbc_utf8.ini
# Copy the roots.pem file to the .so directory to run test cases.
cp ci/etc/roots.pem /Users/runner/work/cpp-bigquery-odbc/cpp-bigquery-odbc/cmake-out/google/cloud/odbc/roots.pem
export ODBCINI=/Users/runner/work/connection/odbc-driver/odbc.ini
export ODBCINSTINI=/Users/runner/work/connection/odbc-driver/odbcinst.ini
export GOOGLEBIGQUERYODBCINI=/Users/runner/work/connection/googlebigqueryodbc.ini
export GOOGLEBIGQUERYODBCINI_UTF16=/Users/runner/work/connection/googlebigqueryodbc_utf16.ini
export GOOGLEBIGQUERYODBCINI_UTF8=/Users/runner/work/connection/googlebigqueryodbc_utf8.ini
export ODBC_TESTS_DSN="SampleDSNGoogleDriver"
export CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY=/Users/runner/work/connection/key.json
export CPP_BIGQUERY_ODBC_TEST_EXTERNAL_ACCOUNT_AUTH_KEY=/Users/runner/work/connection/external_account_auth_keys.json

mapfile -t ctest_args < <(ctest::common_args)

if [[ "$MATRIX_OS" == "macos-14" ]]; then
  TIMEFORMAT="==> 🕑 CMake test done in %R seconds"
  time {
    io::run ctest "${ctest_args[@]}" --test-dir cmake-out -LE integration-test
  }
fi
