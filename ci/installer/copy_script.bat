@echo off
setlocal enabledelayedexpansion

set "SOURCE_DIR=..\..\build\google\cloud\odbc\Release"
set "DEST_DIR=files\x64\Release"

if not exist "%DEST_DIR%" mkdir "%DEST_DIR%"

set "FILES=abseil_dll.dll cares.dll google_cloud_odbc_bq_driver.dll libcrypto-3-x64.dll libcurl.dll libprotobuf.dll libssl-3-x64.dll re2.dll zlib1.dll"

for %%F in (%FILES%) do (
    if exist "%SOURCE_DIR%\%%F" (
        copy "%SOURCE_DIR%\%%F" "%DEST_DIR%"
        if !errorlevel! equ 0 (
            echo Copied %%F successfully.
        ) else (
            echo Failed to copy %%F.
        )
    ) else (
        echo %%F not found in source directory.
    )
)

echo Copy process completed.
