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
test -n "${CI_DEPENDENCIES_DRIVER_MANAGER_SETUP_SH__:-}" || declare -i CI_DEPENDENCIES_DRIVER_MANAGER_SETUP_SH__=0
if ((CI_DEPENDENCIES_DRIVER_MANAGER_SETUP_SH__++ != 0)); then
  return 0
fi # include guard
CPP_BIGQUERY_ODBC_DRIVER_MANAGER_SETUP_CURR_DIR="$(pwd)"
export CPP_BIGQUERY_ODBC_DRIVER_MANAGER_SETUP_CURR_DIR

export GCS_BUCKET=bq-dev-tools-testing-drivers
export DRIVER_VERSION=3.1.5.1022

# Check gcloud is installed.
echo "Verifying google cloud SDK is installed using GCS Bucket: "${GCS_BUCKET}
if [ "$(gsutil ls gs://${GCS_BUCKET}/odbc | grep -c odbc-driver.${DRIVER_VERSION}.zip)" -eq 0 ]; then
  echo 'ODBC driver not found for download: exiting...'
  exit 1
fi

# Configure connection credentials for the driver.
echo 'Configuring Connection Credentials...'
mkdir -p /opt/odbc-driver/connection
cd /opt/odbc-driver
gcloud secrets versions access latest --secret=service-account-auth-keys --out-file="/opt/odbc-driver/connection/key.json"
echo 'Verifying Connection Keys File Size...'
if [ "$(stat -c%s /opt/odbc-driver/connection/key.json)" -lt 100 ]; then
  echo 'Invalid connection keys: exiting...'
  exit 1
fi

# Install the ODBC Driver
echo 'Installing ODBC Driver...'
gsutil -m cp gs://${GCS_BUCKET}/odbc/odbc-driver.${DRIVER_VERSION}.zip .
unzip -qq odbc-driver.${DRIVER_VERSION}.zip
echo 'Verifying Driver Install Directory...'
if [ "$(
  shopt -s nullglob
  set -- /opt/odbc-driver/*googlebigqueryodbc*
  echo $#
)" -eq 0 ]; then
  echo 'ODBC driver not installed: exiting...'
  exit 1
fi

# Configure environment variables
echo 'Configuring Environment Variables For ODBC Driver...'
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH:-}:/usr/local/lib/
export ODBCINI=/opt/odbc-driver/googlebigqueryodbc/odbc.ini
export ODBCINSTINI=/opt/odbc-driver/googlebigqueryodbc/odbcinst.ini
export SIMBAGOOGLEBIGQUERYODBCINI=/opt/odbc-driver/googlebigqueryodbc/lib/simba.googlebigqueryodbc.ini

cd "$CPP_BIGQUERY_ODBC_DRIVER_MANAGER_SETUP_CURR_DIR"

echo '**** ODBC Driver installation END****'
