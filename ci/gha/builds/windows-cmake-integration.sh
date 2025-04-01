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

#!/bin/bash

# Exit on error, undefined variable, or pipe failure
set -euo pipefail

# Source necessary libraries (assuming they define ctest::common_args and io::run)
# Adjust paths if the script location changes relative to these libs
source "$(dirname "$0")/../../lib/init.sh"
source module ci/gha/builds/lib/windows.sh # May not be needed if only ctest parts are used
source module ci/gha/builds/lib/cmake.sh   # Needed for ctest::common_args

# Ensure CMAKE_OUT is set (should be exported by GHA step)
if [[ -z "${CMAKE_OUT:-}" ]]; then
  echo "Error: CMAKE_OUT environment variable is not set." >&2
  exit 1
fi
# Ensure ODBC_TESTS_DSN is set for the tests
export ODBC_TESTS_DSN="SampleDSN" # Keep this if tests need it

# Gather CTest arguments
mapfile -t ctest_args < <(ctest::common_args)

# --- Run CTest ---
io::log_h1 "Starting Tests (CTest)"

# Check if CMAKE_OUT directory exists before running tests
if [[ ! -d "${CMAKE_OUT}" ]]; then
  echo "Error: CMake output directory '${CMAKE_OUT}' not found. Cannot run tests." >&2
  exit 1
fi

TIMEFORMAT="==> 🕑 CMake test done in %R seconds"
time {
  # Run tests from the build directory, excluding integration tests
  # Assuming 'io::run' handles logging and execution
  io::run ctest "${ctest_args[@]}" --test-dir "${CMAKE_OUT}" -LE integration-test
}

io::log_h1 "Tests Finished (CTest)"
