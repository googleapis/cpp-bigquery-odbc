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

mapfile -t args < <(cmake::common_args)
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

TIMEFORMAT="==> 🕑 CMake test done in %R seconds"
time {
  io::run ctest "${ctest_args[@]}" --test-dir cmake-out -LE integration-test
}
