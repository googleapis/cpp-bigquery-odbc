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

set -euo pipefail

# A simple caching script for Google Cloud Build that saves and restores directories to/from Google Cloud Storage.

function usage() {
  echo "Usage:"
  echo "  $0 restore --bucket_url=<bucket_url> --key=<key> [--fallback_key=<fallback_key>]"
  echo "  $0 save --bucket_url=<bucket_url> --key=<key> --path=<path1> [--path=<path2> ...]"
  exit 1
}

if [[ $# -lt 1 ]]; then
  usage
fi

ACTION="$1"
shift

BUCKET_URL=""
KEY=""
FALLBACK_KEY=""
PATHS=()

# Parse arguments
while [[ $# -gt 0 ]]; do
  case "$1" in
    --bucket_url=*)
      BUCKET_URL="${1#*=}"
      shift
      ;;
    --key=*)
      KEY="${1#*=}"
      shift
      ;;
    --fallback_key=*)
      FALLBACK_KEY="${1#*=}"
      shift
      ;;
    --path=*)
      PATHS+=("${1#*=}")
      shift
      ;;
    *)
      echo "Unknown argument: $1"
      usage
      ;;
  esac
done

if [[ -z "${BUCKET_URL}" || -z "${KEY}" ]]; then
  echo "Error: --bucket_url and --key are required."
  usage
fi

# Remove trailing slashes from bucket URL
BUCKET_URL="${BUCKET_URL%/}"

case "${ACTION}" in
  restore)
    # Try to restore from primary key, then fallback key
    TARGET_URL="${BUCKET_URL}/${KEY}.tar.gz"
    echo "Attempting to restore cache from ${TARGET_URL}..."

    if gsutil -q stat "${TARGET_URL}"; then
      echo "Found cache at primary key. Downloading and extracting..."
      gsutil cp "${TARGET_URL}" - | tar -xzf -
      echo "Cache restored successfully."
    elif [[ -n "${FALLBACK_KEY}" ]]; then
      FALLBACK_URL="${BUCKET_URL}/${FALLBACK_KEY}.tar.gz"
      echo "Primary cache not found. Attempting fallback: ${FALLBACK_URL}..."
      if gsutil -q stat "${FALLBACK_URL}"; then
        echo "Found cache at fallback key. Downloading and extracting..."
        gsutil cp "${FALLBACK_URL}" - | tar -xzf -
        echo "Cache restored successfully from fallback."
      else
        echo "Fallback cache not found. Starting with clean environment."
      fi
    else
      echo "Cache not found. Starting with clean environment."
    fi
    ;;

  save)
    if [[ ${#PATHS[@]} -eq 0 ]]; then
      echo "Error: --path is required for save action."
      usage
    fi

    # Filter paths that actually exist to avoid tar errors
    EXISTING_PATHS=()
    for p in "${PATHS[@]}"; do
      if [[ -e "$p" ]]; then
        EXISTING_PATHS+=("$p")
      else
        echo "Warning: path '$p' does not exist, skipping."
      fi
    done

    if [[ ${#EXISTING_PATHS[@]} -eq 0 ]]; then
      echo "No paths exist to cache. Exiting."
      exit 0
    fi

    TARGET_URL="${BUCKET_URL}/${KEY}.tar.gz"
    echo "Archiving and uploading to ${TARGET_URL}..."
    tar -czf - "${EXISTING_PATHS[@]}" | gsutil cp - "${TARGET_URL}"
    echo "Cache saved successfully."
    ;;

  *)
    echo "Unknown action: ${ACTION}"
    usage
    ;;
esac
