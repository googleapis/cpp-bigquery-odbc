#!/bin/bash
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

# --- Include guard ---
test -n "${CI_DEPENDENCIES_GOOGLE_DRIVER_MANAGER_SETUP_SH__:-}" || declare -i CI_DEPENDENCIES_GOOGLE_DRIVER_MANAGER_SETUP_SH__=0
if ((CI_DEPENDENCIES_GOOGLE_DRIVER_MANAGER_SETUP_SH__++ != 0)); then
  return 0
fi # include guard

# --- Extract version from GitHub tag ---
TAG="${GITHUB_REF#refs/tags/}" # e.g. v0.0.24
VERSION="${TAG#v}"             # e.g. 0.0.24
echo "Version from Git tag: $VERSION"

CPP_GOOGLE_BIGQUERY_ODBC_DRIVER_MANAGER_SETUP_OSX_CURR_DIR="$(pwd)"
export CPP_GOOGLE_BIGQUERY_ODBC_DRIVER_MANAGER_SETUP_OSX_CURR_DIR
export GCS_BUCKET=bq-dev-tools-testing-drivers

# Check gcloud is installed.
echo "Verifying google cloud SDK is installed using GCS Bucket: "${GCS_BUCKET}
if [ "$(gsutil ls gs://${GCS_BUCKET}/odbc | grep -c odbc-driver.zip)" -eq 0 ]; then
  echo 'ODBC driver not found for download: exiting...'
  exit 1
fi

# Configure connection credentials for the driver (folders only, secrets are fetched later to avoid token expiration).
echo 'Configuring Connection Directories...'
mkdir -p /Users/runner/work/connection
mkdir -p /Users/runner/work/connection/odbc-driver
cd /Users/runner/work/connection/odbc-driver

cd "$CPP_GOOGLE_BIGQUERY_ODBC_DRIVER_MANAGER_SETUP_OSX_CURR_DIR"

source "$(dirname "$0")/../../lib/init.sh"
source module ci/gha/builds/lib/cmake.sh

mapfile -t cmake_args < <(cmake::common_args)
cmake_args+=(
  -DODBC_UNIT_TESTING=OFF
  -DODBC_INTEGRATION_TESTING=ON
  -DBQ_DRIVER_INTEGRATION_TESTS=ON
  -DCLIENT_LIBRARY_INTEGRATION_TESTING=OFF
  -DCMAKE_CXX_FLAGS="-I$(brew --prefix libiodbc)/include"
  -DCMAKE_CXX_STANDARD=17
  -DCMAKE_BUILD_TYPE=Release
  -DPROJECT_VERSION="${VERSION}"
)

# --- Run build ---
mapfile -t vcpkg_args < <(cmake::vcpkg_args)

io::log_h1 "Starting Build"
TIMEFORMAT="==> CMake configuration done in %R seconds"
time {
  io::run cmake -B cmake-out "${cmake_args[@]}" "${vcpkg_args[@]}"
}

TIMEFORMAT="==> CMake build done in %R seconds"
time {
  io::run cmake --build cmake-out
}

echo "ODBC Driver Setup END"
