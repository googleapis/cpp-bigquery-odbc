# Copyright 2025 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


param (
    [Parameter(Mandatory = $true)]
    [string]$new_version
)

# Validate version format (e.g., 1.2.0)
if ($new_version -notmatch '^\d+\.\d+\.\d+$') {
    Write-Error "Invalid version format. Use format like 1.2.3"
    exit 1
}

# Build the correct path to Product.wxs
$product_file = Join-Path $PSScriptRoot "Product.wxs"

# Check if the file exists
if (-not (Test-Path $product_file)) {
    Write-Error "Product.wxs not found at $product_file"
    exit 1
}

Write-Host "Updating version to: $new_version"

# Read the file content
$file_content = Get-Content $product_file -Raw

# Regex patterns
$version_pattern = '(?<=<Product\b[^>]*\bVersion=")([^"]+)(?=")'
$product_code_pattern = '<\?define ProductCode = "\{[^}]+\}" \?>'

# Extract current version
$current_version = [regex]::Match($file_content, $version_pattern).Value
if (-not $current_version) {
    Write-Error "Product version attribute not found in the <Product> tag."
    exit 1
}

Write-Host "Current version is: $current_version"

# Compare versions
$current_ver_obj = [version]$current_version
$new_ver_obj = [version]$new_version

# Determine whether to update ProductCode
$skip_product_code_update = $false
if ($new_ver_obj -eq $current_ver_obj) {
    Write-Host "Same version ($new_version) detected. ProductCode will NOT be updated."
    $skip_product_code_update = $true
} else {
    Write-Host "Newer version detected. Generating new ProductCode..."
    $new_product_code = [guid]::NewGuid().ToString("B").ToUpper()
    Write-Host "Generated new ProductCode: $new_product_code"
}

# Replacements
$version_replacement = $new_version

# Flags
$version_updated = $false
$product_code_updated = $false

# Replace version
if ($file_content -match $version_pattern) {
    $file_content = [regex]::Replace($file_content, $version_pattern, $version_replacement)
    Write-Host "Product version successfully updated to $new_version."
    $version_updated = $true
}

# Replace ProductCode
if (-not $skip_product_code_update) {
    if ($file_content -match $product_code_pattern) {
        $product_code_replacement = "<?define ProductCode = `"$new_product_code`" ?>"
        $file_content = [regex]::Replace($file_content, $product_code_pattern, $product_code_replacement)
        Write-Host "ProductCode successfully updated."
        $product_code_updated = $true
    } else {
        Write-Error "ProductCode definition not found in the file."
    }
} else {
    Write-Host "ProductCode left unchanged."
    $product_code_updated = $true  # Treat as "successful" for file write
}

# Validate UpgradeCode exists (do not change it!)
$upgrade_code_pattern = '<\?define UpgradeCode = "\{[^}]+\}" \?>'
if ($file_content -notmatch $upgrade_code_pattern) {
    Write-Error "ERROR: UpgradeCode definition not found! It must be present and fixed for upgrade to work."
    exit 1
} else {
    Write-Host "UpgradeCode is present and unchanged"
}

# Write file back only if successful
if ($version_updated -and $product_code_updated) {
    Set-Content -Path $product_file -Value $file_content -Encoding UTF8
    Write-Host "Product.wxs updated successfully."
} else {
    Write-Error "Update failed. File not written."
    exit 1
}
