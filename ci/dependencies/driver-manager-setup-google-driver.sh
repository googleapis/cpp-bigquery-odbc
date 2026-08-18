#!/usr/bin/env bash
#
# Copyright 2025 Google LLC
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

# ============================================================================
# Google BigQuery ODBC Driver
# ============================================================================

# Make our include guard clean against set -o nounset.
test -n "${CI_DEPENDENCIES_GOOGLE_DRIVER_MANAGER_SETUP_SH__:-}" || \
  declare -i CI_DEPENDENCIES_GOOGLE_DRIVER_MANAGER_SETUP_SH__=0

if ((CI_DEPENDENCIES_GOOGLE_DRIVER_MANAGER_SETUP_SH__++ == 0)); then

  CPP_GOOGLE_BIGQUERY_ODBC_DRIVER_MANAGER_SETUP_CURR_DIR="$(pwd)"
  export CPP_GOOGLE_BIGQUERY_ODBC_DRIVER_MANAGER_SETUP_CURR_DIR

  export GCS_BUCKET=bq-dev-tools-testing-drivers

  # Check Google driver is available.
  echo "Verifying Google BigQuery ODBC driver using GCS Bucket: ${GCS_BUCKET}"

  if [ "$(gsutil ls "gs://${GCS_BUCKET}/odbc" | grep -c 'odbc-driver.zip')" -eq 0 ]; then
    echo 'Google BigQuery ODBC driver not found for download: exiting...'
    exit 1
  fi

  # Configure connection credentials.
  echo 'Configuring Google driver connection credentials...'

  mkdir -p /opt/odbc-driver/connection
  cd /opt/odbc-driver

  gcloud secrets versions access latest \
    --secret=service-account-auth-keys \
    --out-file="/opt/odbc-driver/connection/key.json"

  echo 'Verifying Google driver connection keys file size...'

  if [ "$(stat -c%s /opt/odbc-driver/connection/key.json)" -lt 100 ]; then
    echo 'Invalid connection keys: exiting...'
    exit 1
  fi

  # Configure Google driver environment variables.
  echo 'Configuring environment variables for Google BigQuery ODBC driver...'

  export LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-}:/usr/local/lib/"
  export ODBCSYSINI=/opt/odbc-driver
  export ODBCINI=/opt/odbc-driver/odbc.ini
  export CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY=/opt/odbc-driver/connection/key.json
  export GOOGLEBIGQUERYODBCINI=/opt/odbc-driver/googlebigqueryodbc.ini
  export GOOGLEBIGQUERYODBCINI_UTF16=/opt/odbc-driver/googlebigqueryodbc_utf16.ini
  export GOOGLEBIGQUERYODBCINI_UTF8=/opt/odbc-driver/googlebigqueryodbc_utf8.ini

  cd "$CPP_GOOGLE_BIGQUERY_ODBC_DRIVER_MANAGER_SETUP_CURR_DIR"

  echo '**** Google BigQuery ODBC Driver setup END ****'

fi


# ============================================================================
# Simba ODBC Driver
# ============================================================================

# Make our include guard clean against set -o nounset.
test -n "${CI_DEPENDENCIES_DRIVER_MANAGER_SETUP_SH__:-}" || \
  declare -i CI_DEPENDENCIES_DRIVER_MANAGER_SETUP_SH__=0

if ((CI_DEPENDENCIES_DRIVER_MANAGER_SETUP_SH__++ == 0)); then

  CPP_BIGQUERY_ODBC_DRIVER_MANAGER_SETUP_CURR_DIR="$(pwd)"
  export CPP_BIGQUERY_ODBC_DRIVER_MANAGER_SETUP_CURR_DIR

  export GCS_BUCKET=bq-dev-tools-testing-drivers
  export DRIVER_VERSION=3.3.1.3003

  # Check Simba driver is available.
  echo "Verifying Simba ODBC driver using GCS Bucket: ${GCS_BUCKET}"

  if [ "$(gsutil ls "gs://${GCS_BUCKET}/odbc" | grep -c "odbc-driver.${DRIVER_VERSION}.zip")" -eq 0 ]; then
    echo 'Simba ODBC driver not found for download: exiting...'
    exit 1
  fi

  # Configure connection credentials.
  echo 'Configuring Simba connection credentials...'

  mkdir -p /opt/odbc-driver/connection
  cd /opt/odbc-driver

  gcloud secrets versions access latest \
    --secret=service-account-auth-keys \
    --out-file="/opt/odbc-driver/connection/key.json"

  echo 'Verifying Simba connection keys file size...'

  if [ "$(stat -c%s /opt/odbc-driver/connection/key.json)" -lt 100 ]; then
    echo 'Invalid connection keys: exiting...'
    exit 1
  fi

  # Install Simba ODBC driver.
  echo 'Installing Simba ODBC driver...'

  gsutil -m cp \
    "gs://${GCS_BUCKET}/odbc/odbc-driver.${DRIVER_VERSION}.zip" \
    .

  unzip -qq "odbc-driver.${DRIVER_VERSION}.zip"

  echo 'Verifying Simba driver install directory...'

  if [ "$(
    shopt -s nullglob
    set -- /opt/odbc-driver/*googlebigqueryodbc*
    echo $#
  )" -eq 0 ]; then
    echo 'Simba ODBC driver not installed: exiting...'
    exit 1
  fi

  # Configure Simba environment variables.
  echo 'Configuring environment variables for Simba ODBC driver...'

  export LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-}:/usr/local/lib/"
  export ODBCINI=/opt/odbc-driver/googlebigqueryodbc/odbc.ini
  export ODBCINSTINI=/opt/odbc-driver/googlebigqueryodbc/odbcinst.ini
  export SIMBAGOOGLEBIGQUERYODBCINI=/opt/odbc-driver/googlebigqueryodbc/lib/simba.googlebigqueryodbc.ini

  cd "$CPP_BIGQUERY_ODBC_DRIVER_MANAGER_SETUP_CURR_DIR"

  echo '**** Simba ODBC Driver setup END ****'

fi