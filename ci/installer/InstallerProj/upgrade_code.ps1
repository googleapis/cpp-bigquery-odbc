param (
    [Parameter(Mandatory = $true)]
    [string]$new_version
)

# Validate version format (e.g., 1.2.0.0)
if ($new_version -notmatch '^\d+\.\d+\.\d+\.\d+$') {
    Write-Error "Invalid version format. Use format like 1.2.3.4"
    exit 1
}

# Check if the environment variable is set
if (-not $env:CPP_BIGQUERY_ODBC_REPO_PATH) {
    Write-Error "Environment variable CPP_BIGQUERY_ODBC_REPO_PATH is not set."
    exit 1
}

# Build the correct path to Product.wxs
$product_file = Join-Path $env:CPP_BIGQUERY_ODBC_REPO_PATH "ci\installer\InstallerProj\Product.wxs"

# Check if the file exists
if (-not (Test-Path $product_file)) {
    Write-Error "Product.wxs not found at $product_file"
    exit 1
}

Write-Host "Updating version to: $new_version"

# Generate a new ProductCode (required for major upgrades)
$new_product_code = [guid]::NewGuid().ToString("B").ToUpper()
Write-Host "Generated new ProductCode: $new_product_code"

# Read the file content
$file_content = Get-Content $product_file -Raw

# Regex patterns
$version_pattern = '(?<=<Product\b[^>]*\bVersion=")([^"]+)(?=")'
$product_code_pattern = '<\?define ProductCode = "\{[^}]+\}" \?>'

# Replacements
$version_replacement = $new_version
$product_code_replacement = "<?define ProductCode = `"$new_product_code`" ?>"

# Flags
$version_updated = $false
$product_code_updated = $false

# Replace version
if ($file_content -match $version_pattern) {
    $file_content = [regex]::Replace($file_content, $version_pattern, $version_replacement)
    Write-Host "Product version successfully updated to $new_version."
    $version_updated = $true
} else {
    Write-Error "Product version attribute not found in the <Product> tag."
}

# Replace ProductCode
if ($file_content -match $product_code_pattern) {
    $file_content = [regex]::Replace($file_content, $product_code_pattern, $product_code_replacement)
    Write-Host "ProductCode successfully updated."
    $product_code_updated = $true
} else {
    Write-Error "ProductCode definition not found in the file."
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
