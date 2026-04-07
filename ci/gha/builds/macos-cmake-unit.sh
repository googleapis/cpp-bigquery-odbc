#!/usr/bin/env bash
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
source module ci/gha/builds/lib/macos.sh
source module ci/gha/builds/lib/cmake.sh

if [[ -z "${CMAKE_OUT:-}" ]]; then
  CMAKE_OUT=cmake-out
fi

mapfile -t args < <(cmake::common_args "${CMAKE_OUT}")
mapfile -t vcpkg_args < <(cmake::vcpkg_args)
mapfile -t ctest_args < <(ctest::common_args)

args+=("-DODBC_EXAMPLES=OFF")
args+=("-DGTEST_HAS_ABSL=0")
args+=("-DODBC_UNIT_TESTING=ON")
args+=("-DODBC_INTEGRATION_TESTING=OFF")
args+=("-DCLIENT_LIBRARY_INTEGRATION_TESTING=OFF")
args+=("-DBQ_DRIVER_INTEGRATION_TESTS=OFF")
args+=("-DCMAKE_CXX_STANDARD=20")

if command -v sccache >/dev/null 2>&1; then
  args+=(
    -DCMAKE_CXX_COMPILER_LAUNCHER=sccache
  )
fi

io::log_h1 "Starting macOS Unit Test Build"

TIMEFORMAT="==> 🕑 CMake configuration done in %R seconds"
time {
  io::run cmake "${args[@]}" "${vcpkg_args[@]}"
}

TIMEFORMAT="==> 🕑 CMake build done in %R seconds"
time {
  io::run cmake --build "${CMAKE_OUT}" --parallel
}

export CTEST_OUTPUT_ON_FAILURE=1
export GTEST_COLOR=1

TIMEFORMAT="==> 🕑 Unit tests done in %R seconds"
time {
  io::run ctest \
    "${ctest_args[@]}" \
    --test-dir "${CMAKE_OUT}" \
    -LE integration-test \
    --output-on-failure \
    --parallel 2 \
    --timeout 300 \
    --schedule-random
}
