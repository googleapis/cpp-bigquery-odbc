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

# Make our include guard clean against set -o nounset.
test -n "${CI_CLOUDBUILD_BUILDS_LIB_ODBC_DRIVER_INSTALL_SH__:-}" || declare -i CI_CLOUDBUILD_BUILDS_LIB_ODBC_DRIVER_INSTALL_SH__=0
if ((CI_CLOUDBUILD_BUILDS_LIB_ODBC_DRIVER_INSTALL_SH__++ != 0)); then
  return 0
fi # include guard

export GCS_BUCKET=bq-dev-tools-testing-drivers
export ODBC_DRIVER_VERSION=3.0.5.1011
export ODBC_DRIVER_MSI_NAME=SimbaODBCDriverforGoogleBigQuery64_${ODBC_DRIVER_VERSION}.msi
gsutil -m cp gs://${GCS_BUCKET}/odbc-windows/64/${ODBC_DRIVER_MSI_NAME} .
msiexec /i ${ODBC_DRIVER_MSI_NAME} /qn /l*v install_log.txt 
