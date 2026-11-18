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

# This bash library configures GCS-backed build caches (vcpkg, Bazel, sccache)
# so that Cloud Build, Kokoro, and local Docker builds share the same cache
# layout and environment setup.

# Make our include guard clean against set -o nounset.
test -n "${CI_CLOUDBUILD_BUILDS_LIB_CACHE_SH__:-}" || declare -i CI_CLOUDBUILD_BUILDS_LIB_CACHE_SH__=0
if ((CI_CLOUDBUILD_BUILDS_LIB_CACHE_SH__++ != 0)); then
  return 0
fi # include guard

source module ci/lib/io.sh

# Configures GCS build caches (vcpkg, Bazel, sccache) for the given distro and
# build name when CACHE_BUCKET is set, and prepares VCPKG_DOWNLOADS. Existing
# cache environment variables are preserved so callers can override individual
# caches.
function cache::configure() {
  local distro="${1:-${DISTRO:-}}"
  local build_name="${2:-}"
  local bucket="${CACHE_BUCKET:-}"
  local branch="${BRANCH_NAME:-main}"
  local sanitized_branch="${branch//\//-}"

  if [[ -n "${bucket}" && -n "${distro}" && -n "${build_name}" ]]; then
    local main_cache_id="main-${distro}-${build_name}"
    local branch_cache_id="${sanitized_branch}-${distro}-${build_name}"
    io::log "Configuring GCS build caches in gs://${bucket} (${branch_cache_id})"

    export VCPKG_BINARY_SOURCES="${VCPKG_BINARY_SOURCES:-clear;x-gcs,gs://${bucket}/vcpkg-cache/${main_cache_id},read;x-gcs,gs://${bucket}/vcpkg-cache/${branch_cache_id},readwrite}"
    export VCPKG_DOWNLOADS="${VCPKG_DOWNLOADS:-${HOME:-/root}/.cache/vcpkg/downloads}"
    export BAZEL_REMOTE_CACHE="${BAZEL_REMOTE_CACHE:-https://storage.googleapis.com/${bucket}/bazel-cache/${branch_cache_id}}"
    export SCCACHE_GCS_BUCKET="${SCCACHE_GCS_BUCKET:-${bucket}}"
    export SCCACHE_GCS_KEY_PREFIX="${SCCACHE_GCS_KEY_PREFIX:-sccache/${branch_cache_id}}"
    export SCCACHE_GCS_RW_MODE="${SCCACHE_GCS_RW_MODE:-READ_WRITE}"
    if [[ -z "${SCCACHE_GCS_KEY_PATH:-}" && -n "${GCLOUD_SERVICE_ACCOUNT_KEY_FILE:-}" ]]; then
      export SCCACHE_GCS_KEY_PATH="${GCLOUD_SERVICE_ACCOUNT_KEY_FILE}"
    fi
  fi

  if [[ -n "${VCPKG_DOWNLOADS:-}" ]]; then
    io::log "Creating vcpkg downloads directory: ${VCPKG_DOWNLOADS}"
    mkdir -p "${VCPKG_DOWNLOADS}"
    # Force vcpkg to use the system gcloud storage (and other system tools) by removing
    # any previously cached standalone tools downloaded by vcpkg.
    if [[ -d "${VCPKG_DOWNLOADS}/tools" ]]; then
      io::log "Removing cached vcpkg tools to force system tool usage..."
      rm -rf "${VCPKG_DOWNLOADS}/tools"
    fi
  fi
}
