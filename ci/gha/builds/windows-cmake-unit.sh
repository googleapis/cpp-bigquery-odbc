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
source module ci/gha/builds/lib/windows.sh
source module ci/gha/builds/lib/cmake.sh

if [ "${DRIVER_ARCH:-}" == "x64" ]; then
  export VCPKG_TRIPLET="x64-windows-static"
elif [ "${DRIVER_ARCH:-}" == "x86" ]; then
  export VCPKG_TRIPLET="x86-windows-static"
else
  echo "DRIVER_ARCH must be x64 or x86"
  exit 1
fi

if [[ -z "${CMAKE_OUT:-}" ]]; then
  CMAKE_OUT=cmake-out
fi

mapfile -t args < <(cmake::common_args "${CMAKE_OUT}")
mapfile -t vcpkg_args < <(cmake::vcpkg_args)
mapfile -t ctest_args < <(ctest::common_args)

if [[ $# -gt 1 ]]; then
  args+=("-DCMAKE_BUILD_TYPE=${1}")
  shift
fi

if command -v sccache >/dev/null 2>&1; then
  args+=(
    -DCMAKE_PROJECT_cpp-bigquery-odbc_INCLUDE="$(dirname "$0")/cmake/windows-sccache.cmake"
  )
fi

args+=("-DODBC_EXAMPLES=OFF")
args+=("-DODBC_UNIT_TESTING=ON")
args+=("-DODBC_INTEGRATION_TESTING=OFF")
args+=("-DCLIENT_LIBRARY_INTEGRATION_TESTING=OFF")
args+=("-DBQ_DRIVER_INTEGRATION_TESTS=OFF")
args+=("-DCMAKE_EXE_LINKER_FLAGS=/MANIFEST:NO")

io::log_h1 "Starting Unit Test Build"
printf '%s\n' "${args[@]}"
TIMEFORMAT="==> 🕑 CMake configuration done in %R seconds"
time {
  io::run cmake "${args[@]}" "${vcpkg_args[@]}" -DCMAKE_CXX_STANDARD=20
}

if command -v sccache >/dev/null 2>&1; then
  io::log "Current sccache stats"
  sccache --show-stats
fi

TIMEFORMAT="==> 🕑 CMake build done in %R seconds"
time {
  io::run cmake --build "${CMAKE_OUT}" --parallel 16
}

TIMEFORMAT="==> 🕑 Unit tests done in %R seconds"
time {
  io::run ctest \
    "${ctest_args[@]}" \
    --test-dir "${CMAKE_OUT}" \
    --timeout 300 \
    --output-on-failure \
    --parallel 2 \
    --force-new-ctest-process \
    --schedule-random
}
