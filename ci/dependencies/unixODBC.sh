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
test -n "${CI_DEPENDENCIES_UNIXODBC_SH__:-}" || declare -i CI_DEPENDENCIES_UNIXODBC_SH__=0
if ((CI_DEPENDENCIES_UNIXODBC_SH__++ != 0)); then
  return 0
fi # include guard

readonly CPP_BIGQUERY_ODBC_UNIXODBC_INSTALL_DIR=/var/tmp/unixODBC
mkdir -p $CPP_BIGQUERY_ODBC_UNIXODBC_INSTALL_DIR

readonly CPP_BIGQUERY_ODBC_UNIXODBC_VERSION="2.3.12"
curl -fsSL https://github.com/lurcher/unixODBC/releases/download/${CPP_BIGQUERY_ODBC_UNIXODBC_VERSION}/unixODBC-${CPP_BIGQUERY_ODBC_UNIXODBC_VERSION}.tar.gz |
  tar -zxf - --strip-components=1 &&
  # This is needed because 'BOOL' defined by unixODBC headers causes issues while building google-cloud-cpp
  find . -type f -exec sed -i -r 's/\bBOOL\b/USELESS_BOOL/g' {} + &&
  ./configure --enable-gui=no --enable-drivers=no &&
  make &&
  make install -j "$(nproc)"

# Copy the needed files to a default c++ include dir
cp include/* /usr/include

rm -rf $CPP_BIGQUERY_ODBC_UNIXODBC_INSTALL_DIR
