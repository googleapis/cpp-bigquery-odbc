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

Write-Output "Installation completedfsfd. Log contents:"
$CI_CLOUDBUILD_BUILDS_LIB_ODBC_DRIVER_INSTALL_SH__ = $null
if (-not [string]::IsNullOrEmpty($env:CI_CLOUDBUILD_BUILDS_LIB_ODBC_DRIVER_INSTALL_SH__)) {
    $CI_CLOUDBUILD_BUILDS_LIB_ODBC_DRIVER_INSTALL_SH__ = 0
}

# Include Guard (Similar to "if ((...)); then return 0; fi" in bash)
if ($CI_CLOUDBUILD_BUILDS_LIB_ODBC_DRIVER_INSTALL_SH__ -ne $null -and ++$CI_CLOUDBUILD_BUILDS_LIB_ODBC_DRIVER_INSTALL_SH__ -ne 0) {
    return 0
}

# === INSTALL WINDOWS DEBUGGING TOOLS (gflags.exe) ===
Write-Output "==== Installing Windows Debugging Tools for memory diagnostics ===="

# Check if gflags.exe is already available
$gflagsPath = Get-Command gflags.exe -ErrorAction SilentlyContinue
if (-not $gflagsPath) {
    Write-Output "gflags.exe not found. Installing Windows Debugging Tools..."
    
    # Download Windows SDK installer
    $sdkUrl = "https://go.microsoft.com/fwlink/p/?linkid=2120843"
    $sdkSetup = "winsdksetup.exe"
    $sdkDir = ".\winsdk_tmp"
    $installPath = ".\debug_tools_install"
    
    # Create directories
    New-Item -ItemType Directory -Force -Path $sdkDir | Out-Null
    New-Item -ItemType Directory -Force -Path $installPath | Out-Null
    
    # Download SDK installer
    Write-Output "Downloading Windows SDK installer..."
    $sdkSetupPath = Join-Path $sdkDir $sdkSetup
    Invoke-WebRequest -Uri $sdkUrl -OutFile $sdkSetupPath
    
    # Extract layout first (required for silent install)
    Write-Output "Extracting SDK layout..."
    $layoutPath = Join-Path $sdkDir "WinSDKLayout"
    Start-Process -FilePath $sdkSetupPath -ArgumentList "/layout `"$layoutPath`" /quiet" -Wait -NoNewWindow
    
    # Install Debugging Tools from layout
    Write-Output "Installing Debugging Tools..."
    $layoutSetup = Join-Path $layoutPath "winsdksetup.exe"
    if (Test-Path $layoutSetup) {
        Start-Process -FilePath $layoutSetup -ArgumentList "/features OptionId.WindowsDesktopDebuggers /quiet /norestart /installpath `"$installPath`"" -Wait -NoNewWindow
        
        # Find gflags.exe in the installation
        $gflagsExe = Get-ChildItem -Path $installPath -Recurse -Name "gflags.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($gflagsExe) {
            $gflagsFullPath = Join-Path $installPath $gflagsExe
            Write-Output "Found gflags.exe at: $gflagsFullPath"
            
            # Copy to a known location and add to PATH
            $debugToolsDir = ".\debug_tools"
            New-Item -ItemType Directory -Force -Path $debugToolsDir | Out-Null
            Copy-Item $gflagsFullPath -Destination $debugToolsDir
            
            # Add to PATH for this session
            $env:PATH = "$debugToolsDir;$env:PATH"
            Write-Output "gflags.exe installed and added to PATH"
        } else {
            # After running the installer...
$gflagsPaths = @(
    "C:\Program Files (x86)\Windows Kits\10\Debuggers\x86\gflags.exe",
    "C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\gflags.exe"
)
$foundGflags = $false
foreach ($path in $gflagsPaths) {
    if (Test-Path $path) {
        Write-Output "Found gflags.exe at $path"
        # Optionally copy to your workspace or add to PATH
        $foundGflags = $true
    }
}
if (-not $foundGflags) {
    Write-Warning "gflags.exe not found in default locations after installation."
}

            Write-Warning "gflags.exe not found after installation. Listing installation directory:"
            Get-ChildItem -Path $installPath -Recurse | Write-Output
        }
    } else {
        Write-Warning "Layout setup not found at $layoutSetup"
    }
} else {
    Write-Output "gflags.exe already available at: $($gflagsPath.Source)"
}
# === VERIFY GFLAGS INSTALLATION ===
Write-Output "==== Verifying gflags.exe installation ===="
$finalGflagsPath = Get-Command gflags.exe -ErrorAction SilentlyContinue
if ($finalGflagsPath) {
    Write-Output "gflags.exe is available at: $($finalGflagsPath.Source)"
    Write-Output "gflags.exe version info:"
    & gflags.exe /?
} else {
    Write-Warning "gflags.exe is still not available in PATH"
}

Get-Content $logFilePath | Write-Output

