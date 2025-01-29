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

source "$(dirname "$0")/../../lib/init.sh"
source module ci/install-dependencies.sh

source module ci/cloudbuild/builds/lib/cmake.sh
source module ci/cloudbuild/builds/lib/bazel.sh
source module ci/cloudbuild/builds/lib/secrets.sh
source module ci/cloudbuild/builds/lib/unit-tests.sh
source module ci/lib/io.sh

# echo "HOME is set to: $HOME"
# git -C $HOME clone https://github.com/microsoft/vcpkg
# export VCPKG_ROOT=$HOME/vcpkg


# bash "$VCPKG_ROOT/bootstrap-vcpkg.sh"
# echo "Current working directory is: $(pwd)"
# echo "VCPKG_ROOT is set to: $VCPKG_ROOT"
# echo "VCPKG_BINARY_SOURCES=$VCPKG_BINARY_SOURCES"
# "$VCPKG_ROOT/vcpkg" install
# echo "done processing vcpkg"

# This runs all the unit tests
mapfile -t args < <(bazel::common_args)
mapfile -t unit_tests_args < <(unit_tests::bazel_args)
mapfile -t secrets_bazel < <(secrets::bazel_args)

io::run bazel test "${args[@]}" "${secrets_bazel[@]}" "${unit_tests_args[@]}" --test_tag_filters=unit-tests ...

# Run the integration tests
mapfile -t cmake_args < <(cmake::common_args)

BUILD_DIR="/opt/odbc-driver"
# This is the name of DSN set in odbc.ini
export ODBC_TESTS_DSN="SampleDSNGoogleDriver"
export CPP_BIGQUERY_ODBC_TEST_TABLE_PREFIX=${TRIGGER_NAME//[-:;.,?]/_}_${BRANCH_NAME//[-:;.,?]/_}

io::run cmake -B "$BUILD_DIR" \
  "${cmake_args[@]}" \
  -DCMAKE_CXX_STANDARD=17 \
  -DODBC_INTEGRATION_TESTING=ON \
  -DBQ_DRIVER_INTEGRATION_TESTS=ON \
  -DBUILD_SHARED_LIBS=ON \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DODBC_UNIT_TESTING=OFF \
  -DCLIENT_LIBRARY_INTEGRATION_TESTING=OFF

io::run cmake --build cmake-out

mapfile -t ctest_args < <(ctest::common_args)
io::run env -C "$BUILD_DIR" ctest "${ctest_args[@]}"
