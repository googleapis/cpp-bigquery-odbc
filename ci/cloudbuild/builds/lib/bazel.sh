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

# This bash library has various helper functions for our bazel-based builds
# and automatically pre-fetches all dependencies for the project.

# Make our include guard clean against set -o nounset.
test -n "${CI_CLOUDBUILD_BUILDS_LIB_BAZEL_SH__:-}" || declare -i CI_CLOUDBUILD_BUILDS_LIB_BAZEL_SH__=0
if ((CI_CLOUDBUILD_BUILDS_LIB_BAZEL_SH__++ != 0)); then
  return 0
fi # include guard

source module ci/lib/io.sh

# Selects a default bazel version, though individual builds can override this.
: "${USE_BAZEL_VERSION:="7.4.1"}"
export USE_BAZEL_VERSION

# 🚨 Cloud Build ARM64 rule:
# Never execute bazel or bazelisk here.
# Bazelisk downloads amd64 Bazel binaries internally.
# This WILL crash with Exec format error.

if [[ -n "${GOOGLE_CLOUD_BUILD:-}" ]]; then
  io::log "Cloud Build ARM64 detected — skipping bazel/bazelisk execution"
else
  io::log "Non-Cloud Build environment — bazel handled elsewhere"
fi

###############################################################################
# Bazel prefetch — DISABLED on ARM64 Cloud Build
###############################################################################

io::log "Skipping bazel dependency prefetch (ARM64-safe)"

# Outputs a list of args that should be given to all bazel invocations. To read
# this into an array use `mapfile -t my_array < <(bazel::common_args)`
function bazel::common_args() {
  function should_cache_test_results() {
    # Disables test caching on ci and daily builds to surface flaky tests.
    # Enables test caching on other builds to avoid surfacing unrelated flakes.
    case "${TRIGGER_TYPE}" in
      ci | daily)
        echo no
        ;;
      *)
        echo auto
        ;;
    esac
  }
  local args=(
    "--test_output=errors"
    "--verbose_failures=true"
    "--keep_going"
    "--experimental_convenience_symlinks=ignore"
    "--cache_test_results=$(should_cache_test_results)"
  )
  if [[ -n "${BAZEL_REMOTE_CACHE:-}" ]]; then
    args+=("--remote_cache=${BAZEL_REMOTE_CACHE}")
    args+=("--google_default_credentials")
    # See https://docs.bazel.build/versions/main/remote-caching.html#known-issues
    # and https://github.com/bazelbuild/bazel/issues/3360
    args+=("--experimental_guard_against_concurrent_changes")
  fi
  printf "%s\n" "${args[@]}"
}
