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

cp ci/gha/builds/lib/odbc_osx.ini /Users/runner/work/connection/odbc-driver/odbc.ini
cp ci/gha/builds/lib/odbcinst_osx.ini /Users/runner/work/connection/odbc-driver/odbcinst.ini
cp ci/gha/builds/lib/google.googlebigqueryodbc.ini /Users/runner/work/connection/google.googlebigqueryodbc.ini
export ODBCINI=/Users/runner/work/connection/odbc-driver/odbc.ini
export ODBCINSTINI=/Users/runner/work/connection/odbc-driver/odbcinst.ini
export GOOGLEBIGQUERYODBCINI=/Users/runner/work/connection/google.googlebigqueryodbc.ini
export ODBC_TESTS_DSN="SampleDSNGoogleDriver"
export CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY=/Users/runner/work/connection/key.json

mapfile -t ctest_args < <(ctest::common_args)

if [[ "$MATRIX_OS" == "macos-14" ]]; then
  TIMEFORMAT="==> 🕑 CMake test done in %R seconds"
  time {
    io::run ctest "${ctest_args[@]}" --test-dir cmake-out -LE integration-test
  }
fi
