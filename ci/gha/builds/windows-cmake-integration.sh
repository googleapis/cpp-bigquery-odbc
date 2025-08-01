#!/usr/bin/env bash
#
# Copyright 2024 Google LLC
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

# Set VCPKG_TRIPLET based on DRIVER_ARCH
if [ "${DRIVER_ARCH:-}" == "x64" ]; then
  export VCPKG_TRIPLET="x64-windows-static-md"
elif [ "${DRIVER_ARCH:-}" == "x86" ]; then
  export VCPKG_TRIPLET="x86-windows-static-md"
fi

source "$(dirname "$0")/../../lib/init.sh"
source module ci/gha/builds/lib/windows.sh
source module ci/gha/builds/lib/cmake.sh

export ODBC_TESTS_DSN="SampleDSN"


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
    # sccache requires specific workarounds with MSVC.
    -DCMAKE_PROJECT_cpp-bigquery-odbc_INCLUDE="$(dirname "$0")/cmake/windows-sccache.cmake"
  )
fi

# Disable manifest [[1]] generation.  These are known to cause flakes in CI
# systems [[2]], and we do not need manifests for our purposes.
#
# [1]: https://learn.microsoft.com/en-us/windows/win32/sbscs/manifests
# [2]: https://stackoverflow.com/questions/3775406
args+=("-DCMAKE_EXE_LINKER_FLAGS=/MANIFEST:NO")

args+=("-DODBC_EXAMPLES=OFF")
args+=("-DODBC_INTEGRATION_TESTING=ON")
args+=("-DCLIENT_LIBRARY_INTEGRATION_TESTING=OFF")
args+=("-DODBC_UNIT_TESTING=OFF")

# We use our driver or the existing one based on BUILD_SHARD env
if [ "$BUILD_SHARD" == "Core" ]; then
  args+=("-DBQ_DRIVER_INTEGRATION_TESTS=OFF")
else
  args+=("-DBQ_DRIVER_INTEGRATION_TESTS=ON")
fi

io::log_h1 "Starting Build"
TIMEFORMAT="==> 🕑 CMake configuration done in %R seconds"
time {
  # Always run //google/cloud:status_test in case the list of targets has
  # no unit tests.
  io::run cmake "${args[@]}" "${vcpkg_args[@]}" -DCMAKE_CXX_STANDARD=20
}

if command -v sccache >/dev/null 2>&1; then
  io::log "Current sccache stats"
  sccache --show-stats
fi

TIMEFORMAT="==> 🕑 CMake build done in %R seconds"
time {
  # Always run //google/cloud:status_test in case the list of targets has
  # no unit tests.
  io::run cmake --build "${CMAKE_OUT}" --parallel 16
}

if [ "$BUILD_SHARD" == "BqDriver" ] && [ "$DRIVER_ARCH" == "x64" ]; then
  for file in "${CMAKE_OUT}"/google/cloud/odbc/*.dll; do
    cp "$file" "C:\Program Files\Simba ODBC Driver for Google BigQuery\lib"
    echo "Copied $file to C:\Program Files\Simba ODBC Driver for Google BigQuery\lib"
  done
  cp "${CMAKE_OUT}"/google/cloud/odbc/google_cloud_odbc_bq_driver.dll "C:\Program Files\Simba ODBC Driver for Google BigQuery\lib\GoogleBigQueryODBC_sb64.dll"
  echo "Copied ${CMAKE_OUT}/google/cloud/odbc/google_cloud_odbc_bq_driver.dll to C:\Program Files\Simba ODBC Driver for Google BigQuery\lib\GoogleBigQueryODBC_sb64.dll"
  echo "Files in C:\Program Files\Simba ODBC Driver for Google BigQuery\lib:"
  cmd.exe /C dir "C:\Program Files\Simba ODBC Driver for Google BigQuery\lib"
  # Dumpbin output for google_cloud_odbc_bq_driver.dll
  # Run dumpbin, redirect output to a file, then type the file to show output on console
  DUMPBIN_EXE="C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\VC\\Tools\\MSVC\\<version>\\bin\\Hostx64\\x64\\dumpbin.exe"
  DUMPBIN_OUTPUT_FILE="C:\\temp\\dumpbin_output_x64.txt"
  cmd.exe /C "\"$DUMPBIN_EXE\" /exports \"C:\\Program Files\\Simba ODBC Driver for Google BigQuery\\lib\\GoogleBigQueryODBC_sb64.dll\" > \"$DUMPBIN_OUTPUT_FILE\""
  echo "Dumpbin output for google_cloud_odbc_bq_driver.dll:"
  cmd.exe /C "type \"$DUMPBIN_OUTPUT_FILE\""
  cmd.exe /C "del \"$DUMPBIN_OUTPUT_FILE\""
fi

if [ "$BUILD_SHARD" == "BqDriver" ] && [ "$DRIVER_ARCH" == "x86" ]; then
  for file in "${CMAKE_OUT}"/google/cloud/odbc/*.dll; do
    # Skip abseil_dll.dll for x86
    if [[ "$(basename "$file")" == "abseil_dll.dll" ]]; then
      continue
    fi
    cp "$file" "C:\Program Files (x86)\Simba ODBC Driver for Google BigQuery\lib"
    echo "Copied $file to C:\Program Files (x86)\Simba ODBC Driver for Google BigQuery\lib"
  done
  cp "${CMAKE_OUT}"/google/cloud/odbc/google_cloud_odbc_bq_driver.dll "C:\Program Files (x86)\Simba ODBC Driver for Google BigQuery\lib\GoogleBigQueryODBC_sb32.dll"
  echo "Copied ${CMAKE_OUT}/google/cloud/odbc/google_cloud_odbc_bq_driver.dll to C:\Program Files (x86)\Simba ODBC Driver for Google BigQuery\lib\GoogleBigQueryODBC_sb32.dll"

  # Delete abseil_dll.dll from the target directory if it somehow exists there
  cmd.exe /C "del \"C:\Program Files (x86)\Simba ODBC Driver for Google BigQuery\lib\abseil_dll.dll\""
  echo "Deleted abseil_dll.dll from C:\Program Files (x86)\Simba ODBC Driver for Google BigQuery\lib"

  # Also delete abseil_dll.dll from the source directory
  rm -f "${CMAKE_OUT}/google/cloud/odbc/abseil_dll.dll"
  echo "Deleted abseil_dll.dll from source directory ${CMAKE_OUT}/google/cloud/odbc/"

  echo "Files in C:\Program Files (x86)\Simba ODBC Driver for Google BigQuery\lib:"
  cmd.exe /C dir "C:\Program Files (x86)\Simba ODBC Driver for Google BigQuery\lib"

  # Run dumpbin, redirect output to a file, then type the file to show output on console
  DUMPBIN_EXE="C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\VC\\Tools\\MSVC\\<version>\\bin\\Hostx86\\x86\\dumpbin.exe"
  DUMPBIN_OUTPUT_FILE="C:\\temp\\dumpbin_output_x86.txt"
  cmd.exe /C "\"$DUMPBIN_EXE\" /exports \"C:\\Program Files (x86)\Simba ODBC Driver for Google BigQuery\lib\GoogleBigQueryODBC_sb32.dll\" > \"$DUMPBIN_OUTPUT_FILE\""
  echo "Dumpbin output for google_cloud_odbc_bq_driver.dll:"
  cmd.exe /C "type \"$DUMPBIN_OUTPUT_FILE\""

  # Optionally delete the dumpbin output file after displaying
  cmd.exe /C "del \"$DUMPBIN_OUTPUT_FILE\""
fi


TIMEFORMAT="==> 🕑 CMake test done in %R seconds"
time {
  io::run ctest "${ctest_args[@]}" --test-dir "${CMAKE_OUT}" -LE integration-test
}
