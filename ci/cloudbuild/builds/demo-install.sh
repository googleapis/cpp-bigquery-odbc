#!/bin/bash
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
#
# This build script serves two main purposes:
#
# 1. It demonstrates to users how to install `google-cloud-cpp`. We will
#    extract part of this script into user-facing markdown documentation.
# 2. It verifies that the installed artifacts work by compiling and running the
#    quickstart programs against the installed artifacts.

set -euo pipefail

source "$(dirname "$0")/../../lib/init.sh"
source module ci/cloudbuild/builds/lib/cmake.sh
source module ci/lib/io.sh

# We cannot use `cmake --install` below. That flag was introduced
# in CMake == 3.15, and we use (and tell folks to use this code)
# with versions as old as 3.5.

cmake_config_testing_details=(
  -DODBC_BUILD_TESTING=OFF
)
## [BEGIN packaging.md]
# Pick a location to install the artifacts, e.g., `/usr/local` or `/opt`
PREFIX="${HOME}/cpp-bigquery-odbc-installed"
cmake -H. -Bcmake-out \
  "${cmake_config_testing_details[@]}"
cmake --build cmake-out -- -j "$(nproc)"
cmake --build cmake-out --target install
## [DONE packaging.md]
