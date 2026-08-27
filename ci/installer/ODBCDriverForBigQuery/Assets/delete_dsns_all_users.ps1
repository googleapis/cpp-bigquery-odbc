param (
    [string]$DriverName = "ODBC Driver for BigQuery",
    [string[]]$SystemDsnRoots = @(
        "Registry::HKEY_LOCAL_MACHINE\SOFTWARE\ODBC\ODBC.INI",
        "Registry::HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\ODBC\ODBC.INI"
    ),
    [string]$Platform = ""
)

# Identify which DLL we are targeting for cleanup based on architecture
if ($Platform -eq "x64") {
    $TargetDll = "google_cloud_odbc_bq_driver64.dll"
} else {
    $TargetDll = "google_cloud_odbc_bq_driver32.dll"
}

# Get all user SIDs under HKEY_USERS, excluding system _Classes keys
$all_sids = Get-ChildItem "Registry::HKEY_USERS" | Where-Object {
    $_.Name -notmatch "_Classes$"
}

foreach ($sid_entry in $all_sids) {
    $sid = ($sid_entry.Name -split '\\')[-1]  # Extract SID

    $dsn_path_root = "Registry::HKEY_USERS\$sid\Software\ODBC\ODBC.INI"
    $odbc_sources_path = "$dsn_path_root\ODBC Data Sources"

    if (Test-Path $odbc_sources_path) {
        try {
            $dsns = Get-ItemProperty -Path $odbc_sources_path
            foreach ($property in $dsns.PSObject.Properties) {
                $name = $property.Name
                if ($name -notmatch "^PS.*") {
                    $driver = $property.Value

                    if ($driver -eq $DriverName) {
                        # Verify the physical DLL mapped to this DSN
                        $dsnKey = "$dsn_path_root\$name"
                        if (Test-Path $dsnKey) {
                            $driverPath = (Get-ItemProperty -Path $dsnKey -Name "Driver" -ErrorAction SilentlyContinue).Driver
                            
                            if ($driverPath -match $TargetDll) {
                                Remove-Item -Path $dsnKey -Recurse -Force -ErrorAction SilentlyContinue
                                Remove-ItemProperty -Path $odbc_sources_path -Name $name -ErrorAction SilentlyContinue
                            }
                        }
                    }
                }
            }
        } catch {
        }
    }
}

function Load_UserHive {
    param (
        [string]$Sid,
        [string]$UserProfilePath
    )
    $ntuser_dat = Join-Path $UserProfilePath "NTUSER.DAT"
    if (Test-Path $ntuser_dat) {
        reg load "HKU\$Sid" "$ntuser_dat" | Out-Null
    }
}

function Unload_UserHive {
    param (
        [string]$Sid
    )
    reg unload "HKU\$Sid" | Out-Null
}

# User DSNs (HKU)
$hku_path = "Registry::HKEY_USERS"
$sids = Get-ChildItem -Path $hku_path | Where-Object { $_.Name -notmatch "_Classes$" }

# Supplement with user profiles
$user_profiles = Get-ChildItem "C:\Users" | Where-Object {
    Test-Path "$($_.FullName)\NTUSER.DAT"
}

foreach ($user in $user_profiles) {
    $user_path = $user.FullName
    $user_sid = (Get-CimInstance -Class Win32_UserAccount | Where-Object { $_.Name -eq $user.Name }).SID
    if ($user_sid -and -not ($sids.Name -match [regex]::Escape($user_sid))) {
        if (Load_UserHive -Sid $user_sid -UserProfilePath $user_path) {
            $sids += Get-Item "Registry::HKEY_USERS\$user_sid"
        }
    }
}

foreach ($sid in $sids) {
    $sid_name = $sid.PSChildName
    $user_dsn_root = "Registry::HKEY_USERS\$sid_name\Software\ODBC\ODBC.INI"
    $odbc_sources_path = "$user_dsn_root\ODBC Data Sources"

    if (Test-Path $odbc_sources_path) {
        $sources_key = Get-Item -Path $odbc_sources_path
        $sources = $sources_key.GetValueNames()
    } else {
        continue
    }

    foreach ($dsn in $sources) {
        $driver = $sources_key.GetValue($dsn)
        if ($driver -eq $DriverName) {
            $dsnKey = "$user_dsn_root\$dsn"
            if (Test-Path $dsnKey) {
                $driverPath = (Get-ItemProperty -Path $dsnKey -Name "Driver" -ErrorAction SilentlyContinue).Driver
                
                if ($driverPath -match $TargetDll) {
                    Remove-Item -Path $dsnKey -Recurse -Force -ErrorAction SilentlyContinue
                    Remove-ItemProperty -Path $odbc_sources_path -Name $dsn -ErrorAction SilentlyContinue
                }
            }
        }
    }

    if ($user_profiles.Name -contains $sid_name) {
        Unload_UserHive -Sid $sid_name
    }
}

foreach ($dsn_root in $SystemDsnRoots) {
    $sources_path = "$dsn_root\ODBC Data Sources"

    if (Test-Path $sources_path) {
        $dsns = Get-ItemProperty -Path $sources_path
        foreach ($property in $dsns.PSObject.Properties) {
            $dsn_name = $property.Name
            $driver = $property.Value

            if ($driver -eq $DriverName) {
                $dsnKey = "$dsn_root\$dsn_name"
                if (Test-Path $dsnKey) {
                    $driverPath = (Get-ItemProperty -Path $dsnKey -Name "Driver" -ErrorAction SilentlyContinue).Driver
                    
                    if ($driverPath -match $TargetDll) {
                        Remove-Item -Path $dsnKey -Recurse -Force -ErrorAction SilentlyContinue
                        Remove-ItemProperty -Path $sources_path -Name $dsn_name -ErrorAction SilentlyContinue
                    }
                }
            }
        }
    }
}
