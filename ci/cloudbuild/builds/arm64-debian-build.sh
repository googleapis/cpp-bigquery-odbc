#!/bin/bash
#
# Copyright 2026 Google LLC
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
source module ci/install-dependencies.sh

source module ci/cloudbuild/builds/lib/cmake.sh
source module ci/cloudbuild/builds/lib/secrets.sh
source module ci/cloudbuild/builds/lib/unit-tests.sh
source module ci/lib/io.sh

ARCH="$(uname -m)"
JOBS="$(nproc)"

export VCPKG_FORCE_SYSTEM_BINARIES=1
export VCPKG_BUILD_TYPE=release
export VCPKG_MAX_CONCURRENCY="${JOBS}"
export CMAKE_BUILD_PARALLEL_LEVEL="${JOBS}"
export VCPKG_DISABLE_METRICS=1
export VCPKG_FEATURE_FLAGS=manifests,versions
export CMAKE_MAKE_PROGRAM=/usr/bin/ninja
export PATH=/usr/bin:$PATH

if command -v apt-get &>/dev/null; then
  apt-get update
  apt-get install -y ninja-build libatomic1
fi

mapfile -t cmake_args < <(cmake::common_args)

BUILD_DIR="/opt/odbc-driver"
export ODBC_TESTS_DSN="SampleDSNGoogleDriver"
export CPP_BIGQUERY_ODBC_TEST_TABLE_PREFIX=${TRIGGER_NAME//[-:;.,?]/_}_${BRANCH_NAME//[-:;.,?]/_}
export ODBCINSTINI=/opt/odbc-driver/odbcinst.ini
export ODBCINI=/opt/odbc-driver/odbc.ini

io::run cmake -B "$BUILD_DIR" \
  "${cmake_args[@]}" \
  -GNinja \
  -DCMAKE_MAKE_PROGRAM=/usr/bin/ninja \
  -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" \
  -DCMAKE_CXX_STANDARD=17 \
  -DODBC_INTEGRATION_TESTING=ON \
  -DBQ_DRIVER_INTEGRATION_TESTS=ON \
  -DODBC_DEMO_TESTING=ON \
  -DODBC_EXAMPLES=ON \
  -DODBC_UNIT_TESTING=OFF \
  -DCLIENT_LIBRARY_INTEGRATION_TESTING=OFF

io::run cmake --build cmake-out

# Copy the roots.pem file to the .so directory to run test cases.
cp ci/etc/roots.pem "cmake-out/google/cloud/odbc/roots.pem"
mapfile -t ctest_args < <(ctest::common_args)
io::run env -C cmake-out ctest "${ctest_args[@]}"
