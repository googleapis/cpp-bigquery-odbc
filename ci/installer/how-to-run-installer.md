# How to Install and Run WiX Toolset

## Prerequisites

1. Install .NET Framework 4.5 or higher(Make sure feature is enabled on windows)
2. Install WiX Toolset v3.11 or later

## Installation Steps

1. Download WiX Toolset from the official GitHub releases page:
   [https://github.com/wixtoolset/wix3/releases](https://github.com/wixtoolset/wix3/releases)

2. Build the project dlls.

```shell
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DODBC_UNIT_TESTING=OFF -DODBC_INTEGRATION_TESTING=ON -DBQ_DRIVER_INTEGRATION_TESTS=ON -DCLIENT_LIBRARY_INTEGRATION_TESTING=OFF -DCMAKE_CXX_STANDARD=20 -DBUILD_SHARED_LIBS=ON

cmake --build build --config Release
```

3. Run the script file "copy_script.bat" to copy the dlls from Build folder to
   the installer directory

## Building the Installer

### Updating the Upgrade Code  (Optional – only needed when upgrading the driver)

1. Make sure the environment variable CPP_BIGQUERY_ODBC_REPO_PATH is set to the
   root of the project before running the script.
2. run the script file "upgrade_code.ps1" with the specified upgraded version.
   eg: ./upgrade_code.ps1 -new_version "1.2.0.0"

### Using MSBuild (Command Line)

1. Open an administrator command prompt
2. Navigate to the directory containing your .wixproj file
3. Run the following command: msbuild is usually present here "C:\\Program
   Files\\Microsoft Visual Studio\\2022\\Community\\MSBuild\\Current\\Bin"

```shell
msbuild InstallerProj.wixproj /p:Configuration=Release /p:Platform=x64
```
