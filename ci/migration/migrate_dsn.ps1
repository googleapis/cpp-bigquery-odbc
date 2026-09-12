# !/usr/bin/env powershell
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


<#
Migration script for Windows ODBC DSNs
- Migrates from "Simba ODBC Driver for Google Bigquery"
- to       "ODBC Driver for BigQuery"

Usage:
    migrate.ps1 -Mode copy -DriverInstaller "GoogleBigQueryODBC.msi"
    migrate.ps1 -Mode replace -DriverInstaller "GoogleBigQueryODBC.msi"
#>

param(
    [Parameter(Mandatory=$true)]
    [ValidateSet("copy","replace")]
    [string]$Mode,

    [Parameter(Mandatory=$true)]
    [string]$DriverInstaller
)

$SimbaDriverName  = "Simba ODBC Driver for Google Bigquery"
$GoogleDriverName = "ODBC Driver for BigQuery"

Write-Host "=== Installing ODBC Driver for BigQuery ==="
Start-Process "msiexec.exe" -ArgumentList "/i `"$DriverInstaller`" /quiet /norestart" -Wait
Write-Host "=== Driver installation complete ===`n"

# Registry paths for DSNs
$DSNRoots = @(
    "HKLM:\SOFTWARE\ODBC\ODBC.INI",
    "HKCU:\SOFTWARE\ODBC\ODBC.INI"
)
$DSNListRoot = "ODBC Data Sources"

function CopyRegistryTree($SourcePath, $TargetPath) {
    if (!(Test-Path $TargetPath)) {
        New-Item -Path $TargetPath | Out-Null
    }

    $props = Get-ItemProperty -Path $SourcePath
    foreach ($p in $props.PSObject.Properties) {
        if ($p.Name -notlike "PS*") {
            Set-ItemProperty -Path $TargetPath -Name $p.Name -Value $p.Value
        }
    }
}

# === DPAPI HEX DECRYPTOR ===
function Decode-DPAPIHex($hex) {
    if ([string]::IsNullOrWhiteSpace($hex)) { return $null }

    Add-Type -AssemblyName System.Security

    # Convert hexadecimal → byte array
    $bytes = for ($i = 0; $i -lt $hex.Length; $i += 2) {
        [Convert]::ToByte($hex.Substring($i, 2), 16)
    }

    # Try LocalMachine first
    try {
        return [System.Security.Cryptography.ProtectedData]::Unprotect(
            $bytes, $null,
            [System.Security.Cryptography.DataProtectionScope]::LocalMachine
        )
    }
    catch {
        # Try CurrentUser if LocalMachine does not work
        return [System.Security.Cryptography.ProtectedData]::Unprotect(
            $bytes, $null,
            [System.Security.Cryptography.DataProtectionScope]::CurrentUser
        )
    }
}

function TestODBCConnection($DSN) {
    Write-Host " → Testing ODBC connection..."
    try {
        $conn = New-Object System.Data.Odbc.OdbcConnection("DSN=$DSN")
        $conn.Open()
        Write-Host " → Connection test: OK" -ForegroundColor Green
        $conn.Close()
    } catch {
        Write-Host " → Connection test: FAILED" -ForegroundColor Red
        Write-Host $_.Exception.Message
    }
}
$SimbaDSNs = @()

foreach ($root in $DSNRoots) {

    $dsnTablePath = Join-Path $root $DSNListRoot
    if (!(Test-Path $dsnTablePath)) { continue }

    $dsns = Get-ItemProperty $dsnTablePath
    foreach ($entry in $dsns.PSObject.Properties) {

        $dsnName = $entry.Name
        $driverValue = $entry.Value

        if ($driverValue -eq $SimbaDriverName) {
            $SimbaDSNs += [PSCustomObject]@{
                Root = $root
                DsnName = $dsnName
                DsnTablePath = $dsnTablePath
            }
        }
    }
}

if ($SimbaDSNs.Count -eq 0) {
    Write-Host "No Simba DSNs found. Nothing to migrate."
    exit
}
elseif ($SimbaDSNs.Count -eq 1) {
    $Chosen = $SimbaDSNs[0]
}
else {
    Write-Host "`nMultiple Simba DSNs found:"
    for ($i=0; $i -lt $SimbaDSNs.Count; $i++) {
        Write-Host " [$($i+1)] $($SimbaDSNs[$i].DsnName)   ($($SimbaDSNs[$i].Root))"
    }

    do {
        $sel = Read-Host "Enter the number of the DSN you want to migrate"
    } until ($sel -as [int] -and $sel -ge 1 -and $sel -le $SimbaDSNs.Count)

    $Chosen = $SimbaDSNs[$sel - 1]
}

Write-Host "`nSelected DSN: $($Chosen.DsnName)"
Write-Host "Location: $($Chosen.Root)`n"

# ===============================================================
# === Continue the original loop logic ONLY for chosen DSN    ===
# ===============================================================

$root = $Chosen.Root
$dsnName = $Chosen.DsnName
$dsnTablePath = $Chosen.DsnTablePath

Write-Host "`nFound Simba DSN: $dsnName"

# Source DSN registry key
$srcDSNKey = Join-Path $root $dsnName
if (!(Test-Path $srcDSNKey)) {
    Write-Warning "DSN entry exists in table but no registry key: $srcDSNKey"
    exit
}

# Determine target DSN values based on mode
if ($Mode -eq "replace") {
    $newDSNName = $dsnName
} else {
    $newDSNName = "${dsnName}_Google"
}

Write-Host " → Target DSN: $newDSNName"
$dstDSNKey = Join-Path $root $newDSNName

# 1. Update ODBC Data Sources table
Set-ItemProperty -Path $dsnTablePath -Name $newDSNName -Value $GoogleDriverName
if ($Mode -eq "replace" -and $newDSNName -eq $dsnName) {
    Set-ItemProperty -Path $dsnTablePath -Name $dsnName -Value $GoogleDriverName
}

# 2. Copy registry values
CopyRegistryTree -SourcePath $srcDSNKey -TargetPath $dstDSNKey

# 3. Update Driver DLL
$googleDriverKey = Get-ChildItem "HKLM:\SOFTWARE\ODBC\ODBCINST.INI" |
    Where-Object { $_.PSChildName -eq $GoogleDriverName }
if (!$googleDriverKey) { Write-Warning "Google driver not found in ODBCINST.INI"; exit }

$googleDLL = (Get-ItemProperty $googleDriverKey.PSPath).Driver
Set-ItemProperty -Path $dstDSNKey -Name "Driver" -Value $googleDLL
Write-Host " → Set Driver DLL to: $googleDLL"

# 3b. Fix TrustedCerts
$driverFolder = Split-Path $googleDLL -Parent
$rootsCert = Join-Path $driverFolder "Assets\roots.pem"
if (Test-Path $rootsCert) {
    Set-ItemProperty -Path $dstDSNKey -Name "TrustedCerts" -Value $rootsCert
    Write-Host " → Updated TrustedCerts to: $rootsCert"
} else {
    Write-Warning " → roots.pem not found in $driverFolder\Assets"
}

# 4. Decrypt KeyFilePath_Enc
$encKeyPath = (Get-ItemProperty $srcDSNKey -ErrorAction SilentlyContinue).KeyFilePath_Enc
if ($encKeyPath) {

    Write-Host " → Decrypting KeyFilePath_Enc..."

    $rawBytes = Decode-DPAPIHex $encKeyPath
    if ($rawBytes) {
        $plainPath = [System.Text.Encoding]::UTF8.GetString($rawBytes)
        Write-Host "   → Decrypted KeyFilePath: $plainPath"

            # Write plain KeyFilePath into new DSN
            Set-ItemProperty -Path $dstDSNKey -Name "KeyFilePath" -Value $plainPath
        } else {
            Write-Warning "   → Failed to decrypt KeyFilePath_Enc"
        }
    }

        # 5. Test DSN connectivity
        TestODBCConnection -DSN $newDSNName

Write-Host "`n=== Migration complete ==="
