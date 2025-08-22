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

# Please see https://clang.llvm.org/docs/MemorySanitizer.html for features of MemorySanitizer.
# The options we are enabling for this sanitizer can be seen in .bazelrc

set -euo pipefail

source "$(dirname "$0")/../../lib/init.sh"
source module ci/install-dependencies.sh

source module ci/cloudbuild/builds/lib/bazel.sh
source module ci/cloudbuild/builds/lib/unit-tests.sh

export CC=clang
export CXX=clang++

mapfile -t args < <(bazel::common_args)
mapfile -t unit_tests_args < <(unit_tests::bazel_args)

# --- Determine project version ---
PROJECT_VERSION=$(git describe --tags --abbrev=0 2>/dev/null | sed 's/^v//' || echo "0.0.0")
echo "Using PROJECT_VERSION=${PROJECT_VERSION}"

args+=("--config=msan")
io::run bazel test "${args[@]}" "${unit_tests_args[@]}" \
  --define VERSION="${PROJECT_VERSION}"\
  --test_tag_filters=integration-test,unit-tests ...
