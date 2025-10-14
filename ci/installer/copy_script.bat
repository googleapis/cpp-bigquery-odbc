@REM Copyright 2025 Google LLC
@REM
@REM Licensed under the Apache License, Version 2.0 (the "License");
@REM you may not use this file except in compliance with the License.
@REM You may obtain a copy of the License at
@REM
@REM     https://www.apache.org/licenses/LICENSE-2.0
@REM
@REM Unless required by applicable law or agreed to in writing, software
@REM distributed under the License is distributed on an "AS IS" BASIS,
@REM WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
@REM See the License for the specific language governing permissions and
@REM limitations under the License.

@echo off
setlocal enabledelayedexpansion

set "SOURCE_DIR=..\..\build\google\cloud\odbc\Release"
set "DEST_DIR=files\x64\Release"

if not exist "%DEST_DIR%" mkdir "%DEST_DIR%"

set "FILES=google_cloud_odbc_bq_driver.dll"

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
