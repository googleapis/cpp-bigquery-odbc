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
ls -l
pwd
cp ci/gha/builds/lib/odbc_osx.ini /Users/runner/work/connection/odbc-driver/odbc.ini
cp ci/gha/builds/lib/odbcinst_osx.ini /Users/runner/work/connection/odbc-driver/odbcinst.ini
export ODBCINI=/Users/runner/work/connection/odbc-driver/odbc.ini
export ODBCINSTINI=/Users/runner/work/connection/odbc-driver/odbcinst.ini
export ODBC_TESTS_DSN="SampleDSNGoogleDriver"
echo $ODBCINI
export CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY=/Users/runner/work/connection/key.json
echo "prinintg files heere========="
echo $CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY
echo "prinintg files end========="
pwd
mapfile -t args < <(cmake::common_args)
args+=(
  -DODBC_UNIT_TESTING=OFF
  -DODBC_INTEGRATION_TESTING=ON
  -DBQ_DRIVER_INTEGRATION_TESTS=ON
  -DCLIENT_LIBRARY_INTEGRATION_TESTING=OFF
  -DBUILD_SHARED_LIBS=ON
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON
  -DCMAKE_CXX_FLAGS="-I$(brew --prefix libiodbc)/include"
  -DCMAKE_CXX_STANDARD=17
)

mapfile -t vcpkg_args < <(cmake::vcpkg_args)
mapfile -t ctest_args < <(ctest::common_args)

io::log_h1 "Starting Build"
TIMEFORMAT="==> 🕑 CMake configuration done in %R seconds"
time {
  io::run cmake "${args[@]}" "${vcpkg_args[@]}"
}

TIMEFORMAT="==> 🕑 CMake build done in %R seconds"
time {
  io::run cmake --build cmake-out
}
echo "CMAKE DONE first"
ls -l
ls -l cmake-out
echo "CMAKE DONE second"
ls -l cmake-out/google/cloud/odbc
TIMEFORMAT="==> 🕑 CMake test done in %R seconds"
time { 
  io::run ctest "${ctest_args[@]}" --test-dir cmake-out -LE integration-test
}
echo "CMAKE DONE HERE"
cd cmake-out
ls -l

