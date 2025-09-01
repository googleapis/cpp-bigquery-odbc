#!/usr/bin/env bash
#
# Cross-platform ODBC Driver Setup (Linux + macOS)
#
# Copyright 2025 Google LLC
# Licensed under the Apache License, Version 2.0
#

set -euo pipefail

# --- Include guard ---
test -n "${CI_DEPENDENCIES_GOOGLE_DRIVER_MANAGER_SETUP_SH__:-}" || declare -i CI_DEPENDENCIES_GOOGLE_DRIVER_MANAGER_SETUP_SH__=0
if ((CI_DEPENDENCIES_GOOGLE_DRIVER_MANAGER_SETUP_SH__++ != 0)); then
  return 0
fi

CURR_DIR="$(pwd)"
export CURR_DIR

# --- Detect platform ---
OS="$(uname -s)"
echo "Detected OS: $OS"

# --- Extract version from GitHub tag ---
TAG="${GITHUB_REF#refs/tags/}" # e.g. v0.0.24
VERSION="${TAG#v}"             # e.g. 0.0.24
echo "Version from Git tag: $VERSION"

# --- Init helpers ---
source "$(dirname "$0")/../lib/init.sh"
source module ci/gha/builds/lib/cmake.sh
source module ci/lib/io.sh

# --- macOS specific setup ---
if [[ "$OS" == "Darwin" ]]; then
  export GCS_BUCKET=bq-dev-tools-testing-drivers

  echo "Verifying Google Cloud SDK / driver artifacts..."
  if [ "$(gsutil ls gs://${GCS_BUCKET}/odbc | grep -c odbc-driver.zip)" -eq 0 ]; then
    echo 'ODBC driver not found for download: exiting...'
    exit 1
  fi

  echo 'Configuring Connection Credentials...'
  mkdir -p /Users/runner/work/connection/odbc-driver
  cd /Users/runner/work/connection/odbc-driver
  gcloud secrets versions access latest --secret=service-account-auth-keys --out-file="key.json"

  file_size=$(stat -f '%z' key.json)
  if [[ $file_size =~ ^[0-9]+$ ]] && [ "$file_size" -lt 100 ]; then
    echo 'Invalid connection keys: exiting...'
    exit 1
  fi

  cd "$CURR_DIR"

  mapfile -t cmake_args < <(cmake::common_args)
  cmake_args+=(
    -DODBC_UNIT_TESTING=OFF
    -DODBC_INTEGRATION_TESTING=ON
    -DBQ_DRIVER_INTEGRATION_TESTS=ON
    -DCLIENT_LIBRARY_INTEGRATION_TESTING=OFF
    -DCMAKE_CXX_FLAGS="-I$(brew --prefix libiodbc)/include"
    -DCMAKE_CXX_STANDARD=17
    -DCMAKE_BUILD_TYPE=Release
    -DPROJECT_VERSION="${VERSION}"
  )

# --- Linux specific setup ---
elif [[ "$OS" == "Linux" ]]; then
  source ci/dependencies/iODBC.sh

  mapfile -t cmake_args < <(cmake::common_args)
  cmake_args+=(
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    -DBUILD_SHARED_LIBS=ON
    -DODBC_DEMO_TESTING=OFF
    -DODBC_UNIT_TESTING=OFF
    -DBQ_DRIVER_INTEGRATION_TESTS=ON
    -DODBC_INTEGRATION_TESTING=ON
    -DCLIENT_LIBRARY_INTEGRATION_TESTING=OFF
    -DCMAKE_BUILD_TYPE=Release
    -DPROJECT_VERSION="${VERSION}"
  )

else
  echo "Unsupported OS: $OS"
  exit 1
fi

# --- Run build ---
mapfile -t vcpkg_args < <(cmake::vcpkg_args)

io::log_h1 "Starting Build on $OS"
TIMEFORMAT="==> CMake configuration done in %R seconds"
time {
  io::run cmake -B cmake-out "${cmake_args[@]}" "${vcpkg_args[@]}"
}

TIMEFORMAT="==> CMake build done in %R seconds"
time {
  io::run cmake --build cmake-out
}

echo "ODBC Driver Setup END ($OS)"
