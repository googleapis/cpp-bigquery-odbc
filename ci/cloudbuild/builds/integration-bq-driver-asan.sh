#!/bin/bash
#
# Copyright 2026 Google LLC
#
# Licensed under the Apache License, Version 2.0
#

set -euo pipefail

source "$(dirname "$0")/../../lib/init.sh"
source module ci/install-dependencies.sh

source module ci/cloudbuild/builds/lib/cmake.sh
source module ci/cloudbuild/builds/lib/io.sh

WORKSPACE_DIR=$(pwd)

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

BUILD_DIR="/opt/odbc-performance"

PERF_BUCKET="gs://bq-dev-tools-testing-drivers/odbc-perf"
DRIVER_BUCKET="gs://bq-dev-tools-testing-drivers/odbc-perf-drivers"

BENCHMARK_ITERATIONS="${BENCHMARK_ITERATIONS:-3}"

CURRENT_BRANCH="${BRANCH_NAME:-main}"

SANITIZED_BRANCH=$(
  echo "${CURRENT_BRANCH}" |
    sed -E 's/[^a-zA-Z0-9._-]/_/g'
)

RESULTS_DIR="${WORKSPACE_DIR}/benchmark-results"

mkdir -p "$RESULTS_DIR"

echo "============================================================"
echo "ODBC Linux Performance Benchmark"
echo "============================================================"
echo "Current branch : ${CURRENT_BRANCH}"
echo "Iterations     : ${BENCHMARK_ITERATIONS}"
echo "Results dir    : ${RESULTS_DIR}"
echo "============================================================"

# ---------------------------------------------------------------------------
# Vcpkg
# ---------------------------------------------------------------------------

VCPKG_VERSION=$(cat /tmp/vcpkg-version.txt)
export VCPKG_VERSION
export VCPKG_ROOT=/vcpkg

echo "Using VCPKG_VERSION=${VCPKG_VERSION}"

git clone \
  --branch "$VCPKG_VERSION" \
  https://github.com/microsoft/vcpkg.git \
  "$VCPKG_ROOT"

cd "$VCPKG_ROOT"

git checkout "$VCPKG_VERSION"

./bootstrap-vcpkg.sh -disableMetrics

cd "$WORKSPACE_DIR"

# ---------------------------------------------------------------------------
# Download Google driver artifacts
# ---------------------------------------------------------------------------

CURRENT_SO="/tmp/libgoogle_cloud_odbc_bq_driver_current.so"
MAIN_SO="/tmp/libgoogle_cloud_odbc_bq_driver_main.so"

echo "Downloading current Google driver..."

gcloud storage cp \
  "${DRIVER_BUCKET}/${SANITIZED_BRANCH}/libgoogle_cloud_odbc_bq_driver.so" \
  "$CURRENT_SO"

echo "Downloading main Google driver..."

gcloud storage cp \
  "${DRIVER_BUCKET}/main/libgoogle_cloud_odbc_bq_driver.so" \
  "$MAIN_SO"

ls -lh \
  "$CURRENT_SO" \
  "$MAIN_SO"

# ---------------------------------------------------------------------------
# Locate Simba driver
# ---------------------------------------------------------------------------
#
# The Simba integration image installs the Simba driver.
# We intentionally don't use another DSN.
#
# We find the .so installed by the Simba environment and save it as an
# artifact that can be selected at runtime.
# ---------------------------------------------------------------------------

echo
echo "Searching for Simba ODBC driver..."

SIMBA_SO=""

while IFS= read -r candidate; do
  if [[ -f "$candidate" ]]; then
    case "$candidate" in
      *google*cloud*odbc*.so|*Simba*.so|*simba*.so)
        SIMBA_SO="$candidate"
        break
        ;;
    esac
  fi
done < <(
  find \
    /opt \
    /usr \
    /workspace \
    -type f \
    -name "*.so" \
    2>/dev/null |
    sort
)

if [[ -z "$SIMBA_SO" ]]; then
  echo "ERROR: Could not locate Simba .so"
  echo
  echo "Available ODBC shared libraries:"
  find \
    /opt \
    /usr \
    /workspace \
    -type f \
    -name "*.so*" \
    2>/dev/null |
    grep -Ei 'odbc|simba' |
    sort ||
    true

  exit 1
fi

echo "Simba driver found:"
echo "  $SIMBA_SO"

SIMBA_COPY="/tmp/libgoogle_cloud_odbc_simba_driver.so"

cp "$SIMBA_SO" "$SIMBA_COPY"

ls -lh "$SIMBA_COPY"

# ---------------------------------------------------------------------------
# Build ONLY performance_test
# ---------------------------------------------------------------------------
#
# BUILD_PERFORMANCE_TEST_ONLY does not build the Google driver.
# It builds performance_test + the small testing utility dependencies.
#
# The driver .so is selected at runtime through the DSN.
# ---------------------------------------------------------------------------

mapfile -t cmake_args < <(cmake::common_args)

io::run cmake \
  -S "$WORKSPACE_DIR" \
  -B "$BUILD_DIR" \
  "${cmake_args[@]}" \
  -DCMAKE_CXX_STANDARD=17 \
  -DBUILD_PERFORMANCE_TEST_ONLY=ON \
  -DBQ_DRIVER_INTEGRATION_TESTS=ON

io::run cmake \
  --build "$BUILD_DIR" \
  --target performance_test \
  --parallel "$(nproc)"

PERFORMANCE_TEST="$BUILD_DIR/integration_tests/performance_test"

if [[ ! -x "$PERFORMANCE_TEST" ]]; then
  echo "ERROR: performance_test was not created."

  echo "Searching build directory:"
  find "$BUILD_DIR" \
    -type f \
    -name "performance_test*" \
    -print

  exit 1
fi

echo "performance_test:"
ls -lh "$PERFORMANCE_TEST"

# ---------------------------------------------------------------------------
# Configure one DSN
# ---------------------------------------------------------------------------
#
# We use ONE DSN.
#
# Before each benchmark run, replace the driver .so referenced by the DSN.
# ---------------------------------------------------------------------------

DSN_DIR="/tmp/benchmark-odbc"

mkdir -p "$DSN_DIR"

ODBC_INI="$DSN_DIR/odbc.ini"

cat > "$ODBC_INI" <<EOF
[ODBC]
Trace=0

[ODBC Data Sources]
SampleDSNGoogleDriver=ODBC Driver for BigQuery

[SampleDSNGoogleDriver]
Description=ODBC Driver for BigQuery
Driver=/tmp/benchmark-driver/libgoogle_cloud_odbc_bq_driver.so

Catalog=bigquery-devtools-drivers
SQLDialect=1
OAuthMechanism=0
Email=bq-devtools-simba-drivers-test@bigquery-devtools-drivers.iam.gserviceaccount.com
KeyFilePath=/opt/odbc-driver/connection/key.json

AllowLargeResults=0
LargeResultsDataSetId=_bqodbc_temp_tables
LargeResultsTempTableExpirationTime=3600000
EOF

export ODBCINI="$ODBC_INI"

DRIVER_RUNTIME_DIR="/tmp/benchmark-driver"

mkdir -p "$DRIVER_RUNTIME_DIR"

# ---------------------------------------------------------------------------
# Service account
# ---------------------------------------------------------------------------

if [[ -f "/opt/odbc-driver/connection/key.json" ]]; then
  export GOOGLE_APPLICATION_CREDENTIALS="/opt/odbc-driver/connection/key.json"
  export CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY="/opt/odbc-driver/connection/key.json"
else
  echo "ERROR: Service account key was not found:"
  echo "       /opt/odbc-driver/connection/key.json"
  exit 1
fi

export ODBC_TESTS_DSN="SampleDSNGoogleDriver"

# ---------------------------------------------------------------------------
# Run benchmark helper
# ---------------------------------------------------------------------------

run_driver_benchmark() {
  local name="$1"
  local source_so="$2"
  local output_file="$3"

  echo
  echo "============================================================"
  echo "Benchmark: ${name}"
  echo "Driver: ${source_so}"
  echo "Iterations: ${BENCHMARK_ITERATIONS}"
  echo "Output: ${output_file}"
  echo "============================================================"

  if [[ ! -f "$source_so" ]]; then
    echo "ERROR: Driver .so does not exist:"
    echo "$source_so"
    return 1
  fi

  cp \
    "$source_so" \
    "${DRIVER_RUNTIME_DIR}/libgoogle_cloud_odbc_bq_driver.so"

  echo "Runtime driver:"
  ls -lh \
    "${DRIVER_RUNTIME_DIR}/libgoogle_cloud_odbc_bq_driver.so"

  : > "$output_file"

  local test_exit_code=0

  for ((i = 1; i <= BENCHMARK_ITERATIONS; i++)); do
    echo "=== benchmark iteration ${i}/${BENCHMARK_ITERATIONS} ===" \
      | tee -a "$output_file"

    set +e

    "$PERFORMANCE_TEST" \
      >> "$output_file" \
      2>&1

    local run_exit_code=$?

    set -e

    if [[ "$run_exit_code" -ne 0 ]]; then
      echo \
        "WARNING: ${name} iteration ${i} exited with ${run_exit_code}"

      echo \
        "WARNING: ${name} iteration ${i} exited with ${run_exit_code}" \
        >> "$output_file"

      test_exit_code="$run_exit_code"
    fi
  done

  if [[ "$test_exit_code" -ne 0 ]]; then
    echo "ERROR: ${name} benchmark failed."
    return "$test_exit_code"
  fi

  echo
  echo "${name} benchmark completed."

  return 0
}

# ---------------------------------------------------------------------------
# Run Simba
# ---------------------------------------------------------------------------

run_driver_benchmark \
  "Simba" \
  "$SIMBA_COPY" \
  "$RESULTS_DIR/simba.txt"

# ---------------------------------------------------------------------------
# Run current Google driver
# ---------------------------------------------------------------------------

run_driver_benchmark \
  "Google Current" \
  "$CURRENT_SO" \
  "$RESULTS_DIR/current_bq.txt"

# ---------------------------------------------------------------------------
# Run main Google driver
# ---------------------------------------------------------------------------

run_driver_benchmark \
  "Google Main" \
  "$MAIN_SO" \
  "$RESULTS_DIR/main_bq.txt"

# ---------------------------------------------------------------------------
# Generate benchmark summary
# ---------------------------------------------------------------------------

SUMMARY_FILE="$RESULTS_DIR/benchmark_summary.txt"

python3 \
  "$WORKSPACE_DIR/ci/cloudbuild/builds/lib/benchmark_results.py" \
  --simba "$RESULTS_DIR/simba.txt" \
  --current "$RESULTS_DIR/current_bq.txt" \
  --main "$RESULTS_DIR/main_bq.txt" \
  --output "$SUMMARY_FILE"

# ---------------------------------------------------------------------------
# Upload results
# ---------------------------------------------------------------------------

RESULTS_GCS_PATH="${PERF_BUCKET}/${SANITIZED_BRANCH}"

echo
echo "Uploading benchmark results..."

gcloud storage cp \
  "$RESULTS_DIR/simba.txt" \
  "${RESULTS_GCS_PATH}/results/"

gcloud storage cp \
  "$RESULTS_DIR/current_bq.txt" \
  "${RESULTS_GCS_PATH}/results/"

gcloud storage cp \
  "$RESULTS_DIR/main_bq.txt" \
  "${RESULTS_GCS_PATH}/results/"

gcloud storage cp \
  "$SUMMARY_FILE" \
  "${RESULTS_GCS_PATH}/results/"

echo
echo "============================================================"
echo "Benchmark completed successfully."
echo "============================================================"
echo
echo "Results:"
echo "${RESULTS_GCS_PATH}/results/simba.txt"
echo "${RESULTS_GCS_PATH}/results/current_bq.txt"
echo "${RESULTS_GCS_PATH}/results/main_bq.txt"
echo "${RESULTS_GCS_PATH}/results/benchmark_summary.txt"