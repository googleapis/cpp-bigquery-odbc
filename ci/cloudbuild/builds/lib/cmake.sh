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

# Include guard
test -n "${CI_CLOUDBUILD_BUILDS_LIB_CMAKE_SH__:-}" || declare -i CI_CLOUDBUILD_BUILDS_LIB_CMAKE_SH__=0
if ((CI_CLOUDBUILD_BUILDS_LIB_CMAKE_SH__++ != 0)); then
  return 0
fi

source module ci/lib/io.sh

io::log "Using CMake version"
cmake --version

export NINJA_STATUS="T+%es [%f/%t] "

###############################################################################
# sccache handling (Cloud Build SAFE)
###############################################################################

SCCACHE_BIN=""

if [[ -n "${CLOUD_BUILD:-}" ]]; then
  io::log "Cloud Build detected (host=amd64) — disabling sccache"
  export SCCACHE_DISABLE=1
else
  if command -v sccache >/dev/null 2>&1; then
    SCCACHE_BIN="$(command -v sccache)"
    io::log "Using local sccache: ${SCCACHE_BIN}"
    "${SCCACHE_BIN}" --zero-stats
  fi
fi

###############################################################################
# CMake helpers
###############################################################################

function cmake::common_args() {
  local args=(
    -GNinja
    -S .
    -B cmake-out
  )

  if [[ -n "${SCCACHE_BIN}" ]]; then
    args+=(
      "-DCMAKE_C_COMPILER_LAUNCHER=${SCCACHE_BIN}"
      "-DCMAKE_CXX_COMPILER_LAUNCHER=${SCCACHE_BIN}"
    )
  fi

  printf "%s\n" "${args[@]}"
}

function ctest::common_args() {
  local args=(
    --output-on-failure
    -j "$(nproc)"
    --progress
  )

  printf "%s\n" "${args[@]}"
}
