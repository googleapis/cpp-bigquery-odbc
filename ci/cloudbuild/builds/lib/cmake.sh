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

# This bash library has various helper functions for our cmake-based builds.

# Make our include guard clean against set -o nounset.
test -n "${CI_CLOUDBUILD_BUILDS_LIB_CMAKE_SH__:-}" || declare -i CI_CLOUDBUILD_BUILDS_LIB_CMAKE_SH__=0
if ((CI_CLOUDBUILD_BUILDS_LIB_CMAKE_SH__++ != 0)); then
  return 0
fi # include guard

source module ci/lib/io.sh

io::log "Using CMake version"
cmake --version

# Adds an elapsed seconds counter at the beginning of the ninja output to help
# us see where builds are taking the most time. See also
# https://ninja-build.org/manual.html#_environment_variables
export NINJA_STATUS="T+%es [%f/%t] "

# This block is run the first (and only) time this script is sourced. It first
# clears the sccache stats. Then it registers an exit handler that will display
# the sccache stats when the calling script exits.
if command -v sccache >/dev/null 2>&1; then
  io::log "Clearing sccache stats"
  if sccache --zero-stats; then
    function show_stats_handler() {
      if [[ "${TRIGGER_TYPE:-}" != "manual" || "${VERBOSE_FLAG:-}" == "true" ]]; then
        io::log "===> sccache stats"
        sccache --show-stats
      fi
    }
    trap show_stats_handler EXIT
  else
    io::log_yellow "sccache failed to initialize; continuing without sccache"
    unset SCCACHE_GCS_BUCKET SCCACHE_GCS_KEY_PREFIX SCCACHE_GCS_RW_MODE SCCACHE_GCS_KEY_PATH
  fi
fi

function cmake::common_args() {
  local args
  args=(
    -GNinja
    -S .
    -B cmake-out
  )
  if [[ -n "${CMAKE_EXTRA_ARGS:-}" ]]; then
    local extra_args
    read -r -a extra_args <<<"${CMAKE_EXTRA_ARGS}"
    args+=("${extra_args[@]}")
  fi
  printf "%s\n" "${args[@]}"
}

function ctest::common_args() {
  local args
  args=(
    # Print the full output on failures
    --output-on-failure
    # Run many tests in parallel, use -j for compatibility with old versions
    -j "$(nproc)"
    # Make the output shorter on interactive tests
    --progress
  )
  if command -v /usr/local/bin/sccache >/dev/null 2>&1; then
    args+=(
      -DCMAKE_CXX_COMPILER_LAUNCHER=/usr/local/bin/sccache
      -DCMAKE_CC_COMPILER_LAUNCHER=/usr/local/bin/sccache
      -DLDFLAGS="-L/opt/homebrew/opt/libiodbc/lib"
      -DCPPFLAGS="-I/opt/homebrew/opt/libiodbc/include"
      -DCMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES=/opt/homebrew/opt/libiodbc/include
    )
  fi
  if [[ -n "${CTEST_REPEAT:-}" ]]; then
    args+=(--repeat "${CTEST_REPEAT}")
  fi
  if [[ -n "${CTEST_OUTPUT_JUNIT:-}" ]]; then
    args+=(--output-junit "${CTEST_OUTPUT_JUNIT}")
  fi
  if [[ -n "${CTEST_EXCLUDE_REGEX:-}" ]]; then
    args+=(-E "${CTEST_EXCLUDE_REGEX}")
  fi
  if [[ -n "${CTEST_EXTRA_ARGS:-}" ]]; then
    local extra_args
    read -r -a extra_args <<<"${CTEST_EXTRA_ARGS}"
    args+=("${extra_args[@]}")
  fi
  printf "%s\n" "${args[@]}"
}
