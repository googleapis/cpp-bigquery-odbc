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

# Make our include guard clean against set -o nounset.
test -n "${CI_DEPENDENCIES_BAZEL_SH__:-}" || declare -i CI_DEPENDENCIES_BAZEL_SH__=0
if ((CI_DEPENDENCIES_BAZEL_SH__++ != 0)); then
  return 0
fi # include guard

curl -o /usr/bin/bazelisk -sSL "https://github.com/bazelbuild/bazelisk/releases/download/v1.24.1/bazelisk-linux-amd64" &&
  chmod +x /usr/bin/bazelisk &&
  ln -s /usr/bin/bazelisk /usr/bin/bazel
