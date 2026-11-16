#!/bin/bash
#
# Copyright 2026 Google LLC
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

# This bash library clones and bootstraps vcpkg at the pinned repository version.

# Make our include guard clean against set -o nounset.
test -n "${CI_CLOUDBUILD_BUILDS_LIB_VCPKG_SH__:-}" || declare -i CI_CLOUDBUILD_BUILDS_LIB_VCPKG_SH__=0
if ((CI_CLOUDBUILD_BUILDS_LIB_VCPKG_SH__++ != 0)); then
  return 0
fi # include guard

source module ci/lib/io.sh

# Clones and bootstraps vcpkg into VCPKG_ROOT (default: /vcpkg) at the version
# pinned in /tmp/vcpkg-version.txt (or ci/etc/vcpkg-version.txt).
function vcpkg::bootstrap() {
  local version_file="/tmp/vcpkg-version.txt"
  if [[ ! -r "${version_file}" ]]; then
    version_file="${PROJECT_ROOT}/ci/etc/vcpkg-version.txt"
  fi

  local vcpkg_version
  vcpkg_version="$(cat "${version_file}")"
  export VCPKG_VERSION="${vcpkg_version}"
  export VCPKG_ROOT="${VCPKG_ROOT:-/vcpkg}"
  io::log "Using VCPKG_VERSION=${VCPKG_VERSION} (VCPKG_ROOT=${VCPKG_ROOT})"

  if [[ -x "${VCPKG_ROOT}/vcpkg" ]]; then
    return 0
  fi

  rm -rf "${VCPKG_ROOT}"
  git clone --branch "${VCPKG_VERSION}" https://github.com/microsoft/vcpkg.git "${VCPKG_ROOT}"
  (
    cd "${VCPKG_ROOT}" || exit 1
    ./bootstrap-vcpkg.sh -disableMetrics
  )
}
