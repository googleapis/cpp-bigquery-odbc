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
$env:ODBC_DRIVER_VERSION = "3.1.2.1004"
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
$env:ODBC_DRIVER_MSI_NAME = "SimbaODBCDriverforGoogleBigQuery${arch}_${env:ODBC_DRIVER_VERSION}.msi"

# Download from Google Cloud Storage (gsutil equivalent)
Write-Output "Downloading $env:ODBC_DRIVER_MSI_NAME from Google Cloud Storage..."
gsutil -m cp gs://${env:GCS_BUCKET}/odbc-windows/${arch}/${env:ODBC_DRIVER_MSI_NAME} . # Assuming gsutil is installed

# Install MSI (with logging to a file)
$installerPath = (Resolve-Path $env:ODBC_DRIVER_MSI_NAME).Path
$logFilePath = "msiexec_install.log"

Write-Output "Installing ODBC driver from $installerPath..."
Start-Process msiexec.exe -ArgumentList "/i `"$installerPath`" /qn /l*v `"$logFilePath`"" -Wait -NoNewWindow

Write-Output "Installation completed. Log contents:"
Get-Content $logFilePath | Write-Output


# --- CMake Configuration and Build Section ---
Write-Output "Starting CMake Build..."


$tempPath = $env:TEMP
$subDirPath = ".build\vcpkg"
$vcpkgFullPath = Join-Path -Path $tempPath -ChildPath $subDirPath
$env:VCPKG_ROOT = $vcpkgFullPath
Write-Output "VCPKG_ROOT set to: $env:VCPKG_ROOT"
$env:CMAKE_OUT="c:\b"


# Define CMake arguments (Translate from bash script)
$cmakeArgs = @()
$vcpkgArgs = @()

# Add vcpkg toolchain file (Assuming VCPKG_ROOT is set correctly)
$vcpkgToolchainFile = Join-Path -Path $env:VCPKG_ROOT -ChildPath "scripts\buildsystems\vcpkg.cmake"
if (Test-Path $vcpkgToolchainFile) {
     $vcpkgArgs += "-DCMAKE_TOOLCHAIN_FILE=$vcpkgToolchainFile"
} else {
    # Minimal warning
    Write-Warning "Vcpkg toolchain file not found at $vcpkgToolchainFile"
}

# Add Build Type (Assuming CMAKE_BUILD_TYPE is set)
if (-not [string]::IsNullOrEmpty($env:CMAKE_BUILD_TYPE)) {
    $cmakeArgs += "-DCMAKE_BUILD_TYPE=$($env:CMAKE_BUILD_TYPE)"
}

# Add sccache flags conditionally
$sccacheCmd = Get-Command sccache -ErrorAction SilentlyContinue
if ($null -ne $sccacheCmd) {
    # Use relative path assuming script location allows finding cmake/windows-sccache.cmake
    # Adjust if needed: $PSScriptRoot\cmake\windows-sccache.cmake
    $sccacheIncludePath = "ci/gha/builds/cmake/windows-sccache.cmake" # Relative to repo root? Adjust if necessary
    $sccacheIncludePath = $sccacheIncludePath.Replace('\', '/')
    $cmakeArgs += "-DCMAKE_PROJECT_cpp-bigquery-odbc_INCLUDE=$sccacheIncludePath"
}

# Add linker flags
$cmakeArgs += "-DCMAKE_EXE_LINKER_FLAGS=/MANIFEST:NO"

# Add Project Specific Flags
$cmakeArgs += "-DODBC_EXAMPLES=OFF" # TODO(b/379091255): Set ON when ready
$cmakeArgs += "-DODBC_INTEGRATION_TESTING=ON"
$cmakeArgs += "-DCLIENT_LIBRARY_INTEGRATION_TESTING=OFF"
$cmakeArgs += "-DODBC_UNIT_TESTING=OFF"

# Add flags based on BUILD_SHARD (Assuming BUILD_SHARD is set)
if ($env:BUILD_SHARD -eq "Core") {
    $cmakeArgs += "-DBQ_DRIVER_INTEGRATION_TESTS=OFF"
} else { # Assumes BqDriver or similar
    $cmakeArgs += "-DBQ_DRIVER_INTEGRATION_TESTS=ON"
    $cmakeArgs += "-DCMAKE_POSITION_INDEPENDENT_CODE=ON"
    $cmakeArgs += "-DBUILD_SHARED_LIBS=ON"
}

# --- Run CMake Configure ---
# Assuming source dir is two levels above script dir (e.g., repo root)
$sourceDir = (Resolve-Path (Join-Path -Path $PSScriptRoot -ChildPath "..\..\..\..")).Path
Write-Output "sourceDir set to: $sourceDir"
# Assuming CMAKE_OUT is set externally
$configureTime = Measure-Command {
    & cmake $sourceDir -B $env:CMAKE_OUT $cmakeArgs $vcpkgArgs "-DCMAKE_CXX_STANDARD=20"
}
Write-Output "==> ? CMake configuration done in $($configureTime.TotalSeconds) seconds"

# Show sccache stats if used
if ($null -ne $sccacheCmd) {
    Write-Output "Current sccache stats:"
    & sccache --show-stats
}

# --- Run CMake Build ---
$buildTime = Measure-Command {
    & cmake --build $env:CMAKE_OUT
}
Write-Output "==> ? CMake build done in $($buildTime.TotalSeconds) seconds"

# --- Conditional DLL Copying ---
# Basic check to prevent errors if output dir doesn't exist
if (Test-Path -Path $env:CMAKE_OUT -PathType Container) {
    $sourceDllPattern = Join-Path -Path $env:CMAKE_OUT -ChildPath "google\cloud\odbc\*.dll"
    $specificDllName = "google_cloud_odbc_bq_driver.dll"
    $specificDllSourcePath = Join-Path -Path $env:CMAKE_OUT -ChildPath "google\cloud\odbc\$specificDllName"

    if ($env:BUILD_SHARD -eq "BqDriver") {
        $targetDir = $null
        $targetSpecificName = $null
        if ($env:DRIVER_ARCH -eq "x64") {
            $targetDir = "C:\Program Files\Simba ODBC Driver for Google BigQuery\lib"
            $targetSpecificName = "GoogleBigQueryODBC_sb64.dll"
        } elseif ($env:DRIVER_ARCH -eq "x86") {
            $targetDir = "C:\Program Files (x86)\Simba ODBC Driver for Google BigQuery\lib"
            $targetSpecificName = "GoogleBigQueryODBC_sb32.dll"
        }

        if ($targetDir -and (Test-Path -Path $targetDir -PathType Container)) {
            # Copy all DLLs
            Get-ChildItem -Path $sourceDllPattern -ErrorAction SilentlyContinue | ForEach-Object {
                Copy-Item -Path $_.FullName -Destination $targetDir -Force
            }
            # Copy and rename the specific driver DLL if it exists
            if (Test-Path $specificDllSourcePath) {
                 $targetSpecificPath = Join-Path -Path $targetDir -ChildPath $targetSpecificName
                 Copy-Item -Path $specificDllSourcePath -Destination $targetSpecificPath -Force
            }
        } elseif ($targetDir) {
             Write-Warning "Target directory '$targetDir' does not exist. Skipping DLL copy."
        }
    }
} else {
     Write-Warning "CMake output directory '$($env:CMAKE_OUT)' not found. Skipping DLL copy."
}

Write-Output "Build script finished."
