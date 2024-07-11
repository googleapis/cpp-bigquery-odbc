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

# Variable Initialization (Similar to "declare -i" in bash)
$CI_CLOUDBUILD_BUILDS_LIB_ODBC_DRIVER_INSTALL_SH__ = $null
if (-not [string]::IsNullOrEmpty($env:CI_CLOUDBUILD_BUILDS_LIB_ODBC_DRIVER_INSTALL_SH__)) {
    $CI_CLOUDBUILD_BUILDS_LIB_ODBC_DRIVER_INSTALL_SH__ = 0
}

# Include Guard (Similar to "if ((...)); then return 0; fi" in bash)
if ($CI_CLOUDBUILD_BUILDS_LIB_ODBC_DRIVER_INSTALL_SH__ -ne $null -and ++$CI_CLOUDBUILD_BUILDS_LIB_ODBC_DRIVER_INSTALL_SH__ -ne 0) {
    return 0 
}

# Set Environment Variables
$env:GCS_BUCKET = "bq-dev-tools-testing-drivers"
$env:ODBC_DRIVER_VERSION = "3.0.5.1011"
$env:ODBC_DRIVER_MSI_NAME = "SimbaODBCDriverforGoogleBigQuery64_${env:ODBC_DRIVER_VERSION}.msi"

# Download from Google Cloud Storage (gsutil equivalent)
Write-Output "Downloading $env:ODBC_DRIVER_MSI_NAME from Google Cloud Storage..." # Log to console
gsutil -m cp gs://${env:GCS_BUCKET}/odbc-windows/64/${env:ODBC_DRIVER_MSI_NAME} . # Assuming gsutil is installed

# Install MSI (with logging to a file)
$installerPath = (Resolve-Path $env:ODBC_DRIVER_MSI_NAME).Path
$logFilePath = "msiexec_install.log"

Write-Output "Installing ODBC driver from $installerPath..." # Log to console
Start-Process msiexec.exe -ArgumentList "/i `"$installerPath`" /qn /l*v `"$logFilePath`"" -Wait -NoNewWindow 

Write-Output "Installation completed. Log contents:" # Log to console
Get-Content $logFilePath | Write-Output

# Download from Google Cloud Storage (gsutil equivalent)
Write-Output "Downloading service account key from Secret Manager..." # Log to console
gcloud secrets versions access latest --secret=service-account-auth-keys --out-file="service_account_auth_keys.json"
Write-Output "Downloaded service account key"


Write-Output "Downloading system_dsn_setup.reg from Google Cloud Storage..." # Log to console
gsutil -m cp gs://${env:GCS_BUCKET}/odbc-windows/64/system_dsn_setup.reg .

Write-Output "Running DSN creation script from $($PWD.Path)\system_dsn_setup.reg ..." # Log to console
reg import .\system_dsn_setup.reg
Write-Output "DSN creation completed. Log contents:" # Log to console
