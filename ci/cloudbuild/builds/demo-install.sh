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

set -euo pipefail

source "$(dirname "$0")/../../lib/init.sh"
source module ci/cloudbuild/builds/lib/cmake.sh
source module ci/lib/io.sh

cmake_config_testing_details=(
  -DODBC_INTEGRATION_TESTING=OFF
)
if command -v /usr/local/bin/sccache >/dev/null 2>&1; then
  cmake_config_testing_details+=(
    -DCMAKE_CXX_COMPILER_LAUNCHER=/usr/local/bin/sccache
  )
fi
## [BEGIN packaging.md]
# Pick a location to install the artifacts, e.g., `/usr/local` or `/opt`
PREFIX="${HOME}/cpp-bigquery-odbc-installed"
cmake -S. -Bcmake-out \
  "${cmake_config_testing_details[@]}"
cmake --build cmake-out -- -j "$(nproc)"
cmake --build cmake-out --target install
## [DONE packaging.md]
