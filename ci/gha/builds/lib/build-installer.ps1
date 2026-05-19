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

$ErrorActionPreference = "Stop"

# Validate environment variables
$driverArch = $env:DRIVER_ARCH
$version = $env:ODBC_GOOGLE_DRIVER_VERSION
$githubRefName = $env:GITHUB_REF_NAME

if ([string]::IsNullOrEmpty($driverArch)) {
    throw "DRIVER_ARCH environment variable is not set or empty."
}
if ([string]::IsNullOrEmpty($version)) {
    throw "ODBC_GOOGLE_DRIVER_VERSION environment variable is not set or empty."
}
if ([string]::IsNullOrEmpty($githubRefName)) {
    throw "GITHUB_REF_NAME environment variable is not set or empty."
}

Write-Output "=== 🚀 Starting WiX Installer Build Process for $driverArch (Version: $version) ==="

# 1. Copy built DLLs to installer directory
$sourceDll = "c:/b/google/cloud/odbc/google_cloud_odbc_bq_driver.dll"
$destDir = "ci/installer/files/$driverArch/Release"

Write-Output "Creating destination directory: $destDir"
$null = New-Item -Path $destDir -ItemType Directory -Force

if (-not (Test-Path -Path $sourceDll)) {
    throw "ERROR: Built DLL not found at: $sourceDll"
}

Write-Output "Moving built DLL to installer files directory..."
Copy-Item -Path $sourceDll -Destination "$destDir/google_cloud_odbc_bq_driver.dll" -Force
Write-Output "DLL copied successfully."

# 2. Prepare Installer Assets
$rootsDest = "ci/installer/ODBCDriverForBigQuery/Assets/roots.pem"
Write-Output "Copying roots.pem to $rootsDest ..."
Copy-Item -Path "ci/etc/roots.pem" -Destination $rootsDest -Force
Write-Output "roots.pem copied successfully."

# Run upgrade script
Write-Output "Running upgrade_code.ps1 with version $version ..."
& ./ci/installer/ODBCDriverForBigQuery/upgrade_code.ps1 -new_version $version

# 3. Build Windows Installer (WiX) using MSBuild
Write-Output "Locating MSBuild..."
$msbuildPath = & "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe
if ([string]::IsNullOrEmpty($msbuildPath)) {
    throw "ERROR: MSBuild.exe could not be located using vswhere."
}
Write-Output "MSBuild found at: $msbuildPath"

Write-Output "Building installer with MSBuild..."
Push-Location "ci/installer/ODBCDriverForBigQuery"
try {
    & $msbuildPath ODBCDriverForBigQuery.wixproj /p:Configuration=Release "/p:Platform=$driverArch"
} finally {
    Pop-Location
}
Write-Output "Installer build completed successfully."

# 4. Rename MSI
$msiDir = "ci/installer/ODBCDriverForBigQuery/bin/Release/en-us"
$newName = "ODBCDriverforBigQuery_windows_${driverArch}_${version}.msi"

Write-Output "Renaming generated MSI..."
$msiFiles = Get-ChildItem -Path $msiDir -Filter *.msi
if ($msiFiles.Count -eq 0) {
    throw "ERROR: No MSI installer file was generated."
}

$originalMsi = $msiFiles[0].FullName
Rename-Item -Path $originalMsi -NewName $newName -Force
Write-Output "MSI successfully renamed to $newName"

# 5. Upload MSI to Google Cloud Storage (GCS)
$gcsBucket = "odbc-integration-builds"
$sanitizedBranch = $githubRefName -replace '[^a-zA-Z0-9\-]', '_'
$destination = "gs://$gcsBucket/$sanitizedBranch/$newName"

Write-Output "Uploading MSI to $destination ..."
gsutil -m cp "$msiDir/$newName" $destination
Write-Output "=== 🎉 Installer Build and Upload Completed Successfully! ==="
