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
test -n "${CI_DEPENDENCIES_IODBC_SH__:-}" || declare -i CI_DEPENDENCIES_IODBC_SH__=0
if ((CI_DEPENDENCIES_IODBC_SH__++ != 0)); then
  return 0
fi # include guard
CPP_BIGQUERY_ODBC_IODBC_CURR_DIR="$(pwd)"
export CPP_BIGQUERY_ODBC_IODBC_CURR_DIR

readonly CPP_BIGQUERY_ODBC_IODBC_INSTALL_DIR=/var/tmp/iODBC
mkdir -p "$CPP_BIGQUERY_ODBC_IODBC_INSTALL_DIR"
cd "$CPP_BIGQUERY_ODBC_IODBC_INSTALL_DIR"

readonly CPP_BIGQUERY_ODBC_IODBC_VERSION="3.52.16"
curl -fsSL "https://github.com/openlink/iODBC/releases/download/v${CPP_BIGQUERY_ODBC_IODBC_VERSION}/libiodbc-${CPP_BIGQUERY_ODBC_IODBC_VERSION}.tar.gz" |
  tar -zxf - --strip-components=1

autoreconf --install
./configure

# Use sudo if available and needed
INSTALL_CMD="make install -j $(nproc)"
if command -v sudo >/dev/null 2>&1; then
  INSTALL_CMD="sudo $INSTALL_CMD"
fi
eval "$INSTALL_CMD"

cd "$CPP_BIGQUERY_ODBC_IODBC_CURR_DIR"

# Clean up
if command -v sudo >/dev/null 2>&1; then
  sudo rm -rf "$CPP_BIGQUERY_ODBC_IODBC_INSTALL_DIR"
else
  rm -rf "$CPP_BIGQUERY_ODBC_IODBC_INSTALL_DIR"
fi
