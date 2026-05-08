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

# Please see https://clang.llvm.org/docs/AddressSanitizer.html for features of AddressSanitizer.
# The options we are enabling for this sanitizer can be seen in .bazelrc

set -euo pipefail

source "$(dirname "$0")/../../lib/init.sh"
source module ci/install-dependencies.sh

source module ci/cloudbuild/builds/lib/bazel.sh
source module ci/cloudbuild/builds/lib/unit-tests.sh

export CC=clang
export CXX=clang++

bazel info output_base
echo "Checking for Abseil dependency conflicts..."

bazel query "somepath(//google/cloud/odbc/..., @abseil-cpp//absl/strings:strings)"
echo "Checking for Abseil dependency conflicts later..."
bazel query "kind(http_archive, //external:*)" | grep absl
echo "Checking for Abseil dependency conflicts later SEC..."
bazel query "deps(//google/cloud/odbc/...) " | grep "@.*absl" | cut -d'/' -f1 | sort -u

# Before running tests, log the Abseil dependency paths to debug the ODR issue

bazel cquery "${args[@]}" "filter('absl', deps(//google/cloud/odbc/...))" --notool_deps || true
echo "Checking for Abseil dependency conflicts end..."

mapfile -t args < <(bazel::common_args)
mapfile -t unit_tests_args < <(unit_tests::bazel_args)
args+=("--config=asan")
bazel test "${args[@]}" "${unit_tests_args[@]}" --test_tag_filters=integration-test,unit-tests ...
