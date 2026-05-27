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
$env:ODBC_DRIVER_VERSION = "3.2.1.3001"
if ([string]::IsNullOrEmpty($env:DRIVER_ARCH)) {
    throw "DRIVER_ARCH environment variable is not set or empty. Please provide a valid architecture."
}

if ($env:DRIVER_ARCH -eq 'x64') {
    $arch = '64'
} elseif ($env:DRIVER_ARCH -eq 'x86') {
    $arch = '32'
} else {
    Write-Error "Invalid architecture: $env:DRIVER_ARCH"
    exit 1
}

if ($env:BUILD_SHARD -eq 'Core') {

    $env:GCS_BUCKET = "bq-dev-tools-testing-drivers"
    $gcsBase = "gs://${env:GCS_BUCKET}/odbc-windows/${arch}/"

    Write-Output "Resolving existing driver from $gcsBase"

    $files = gsutil ls $gcsBase | Select-String "SimbaODBCDriverforGoogleBigQuery${arch}_.*\.msi"

    if (-not $files) {
        Write-Error "No existing driver MSI found in $gcsBase"
        exit 1
    }

    $latest = ($files | Sort-Object)[-1].ToString().Trim()
    $env:ODBC_DRIVER_MSI_NAME = Split-Path $latest -Leaf

    $downloadPath = "${gcsBase}${env:ODBC_DRIVER_MSI_NAME}"

} else {

    $env:GCS_BUCKET = "odbc-integration-builds"

    $branch = "${env:GITHUB_REF_NAME}" -replace '[^a-zA-Z0-9\-]', '_'

    $env:ODBC_DRIVER_MSI_NAME = "ODBCDriverforBigQuery_windows_${env:DRIVER_ARCH}_${env:ODBC_GOOGLE_DRIVER_VERSION}.msi"

    $downloadPath = "gs://${env:GCS_BUCKET}/${branch}/${env:ODBC_DRIVER_MSI_NAME}"
}

# Download from Google Cloud Storage (gsutil equivalent)
Write-Output "Downloading $env:ODBC_DRIVER_MSI_NAME from $downloadPath ..."
gsutil -m cp $downloadPath .

# Install MSI (with logging to a file)
$installerPath = (Resolve-Path $env:ODBC_DRIVER_MSI_NAME).Path
$logFilePath = "msiexec_install.log"

Write-Output "Installing ODBC driver from $installerPath..."
Start-Process msiexec.exe -ArgumentList "/i `"$installerPath`" /qn /l*v `"$logFilePath`"" -Wait -NoNewWindow

Write-Output "Installation completed. Log contents:"
Get-Content $logFilePath | Write-Output
