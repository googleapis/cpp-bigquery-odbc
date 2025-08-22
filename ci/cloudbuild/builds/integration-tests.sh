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
source module ci/install-dependencies.sh

source module ci/cloudbuild/builds/lib/cmake.sh
source module ci/cloudbuild/builds/lib/bazel.sh
source module ci/cloudbuild/builds/lib/secrets.sh
source module ci/cloudbuild/builds/lib/integration.sh
source module ci/lib/io.sh

mapfile -t args < <(bazel::common_args)
mapfile -t integration_args < <(integration::bazel_args)
mapfile -t secrets_bazel < <(secrets::bazel_args)
integration::setup
# --- Determine project version ---
PROJECT_VERSION=$(git describe --tags --abbrev=0 2>/dev/null | sed 's/^v//' || echo "0.0.0")
echo "Using PROJECT_VERSION=${PROJECT_VERSION}"

io::run bazel test //google/cloud/odbc/integration_tests:* \
  "${args[@]}" \
  "${secrets_bazel[@]}" \
  "${integration_args[@]}" \
  --define VERSION="${PROJECT_VERSION}" \
  --cache_test_results=no

# Check there are no build issues with CMake
mapfile -t cmake_args < <(cmake::common_args)

io::run cmake "${cmake_args[@]}" \
  -DCMAKE_CXX_STANDARD=17 \
  -DODBC_INTEGRATION_TESTING=OFF \
  -DODBC_DEMO_TESTING=OFF \
  -DODBC_UNIT_TESTING=OFF \
  -DCLIENT_LIBRARY_INTEGRATION_TESTING=ON
io::run cmake --build cmake-out --clean-first
