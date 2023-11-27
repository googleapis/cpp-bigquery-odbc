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
source module ci/cloudbuild/builds/lib/bazel.sh
source module ci/cloudbuild/builds/lib/integration.sh
source module ci/lib/io.sh

mapfile -t cmake_args < <(cmake::common_args)

# This is the name of DSN set in odbc.ini from simba.zip
export ODBC_TESTS_DSN="SampleDSN"

io::run cmake "${cmake_args[@]}" \
  -DCMAKE_CXX_STANDARD=14 \
  -DODBC_BUILD_TESTING=ON
io::run cmake --build cmake-out

mapfile -t ctest_args < <(ctest::common_args)
io::run env -C cmake-out ctest "${ctest_args[@]}"

# This runs all the unit tests

mapfile -t args < <(bazel::common_args)
mapfile -t integration_args < <(integration::bazel_args)

io::run bazel test "${args[@]}" "${integration_args[@]}" --test_tag_filters=unit-tests ...
