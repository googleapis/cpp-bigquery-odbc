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
source module ci/cloudbuild/builds/lib/secrets.sh
source module ci/lib/io.sh

WORKSPACE_DIR="$(pwd)"

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

BENCHMARK_ITERATIONS="${BENCHMARK_ITERATIONS:-3}"

PERF_DRIVER_BUCKET="gs://bq-dev-tools-testing-drivers/odbc-perf-drivers"

BUILD_DIR="${WORKSPACE_DIR}/cmake-out"
RESULTS_DIR="${WORKSPACE_DIR}/benchmark-results"

mkdir -p "${RESULTS_DIR}"

# ---------------------------------------------------------------------------
# Branch
# ---------------------------------------------------------------------------

BRANCH_NAME="${BRANCH_NAME:-main}"

SANITIZED_BRANCH="$(
  echo "${BRANCH_NAME}" |
    sed -E 's/[^a-zA-Z0-9._-]/_/g'
)"

echo "============================================================"
echo "Linux ODBC Performance Benchmark"
echo "============================================================"
echo "Branch       : ${BRANCH_NAME}"
echo "Iterations   : ${BENCHMARK_ITERATIONS}"
echo "Workspace    : ${WORKSPACE_DIR}"
echo

# ---------------------------------------------------------------------------
# Result files
# ---------------------------------------------------------------------------

CURRENT_RESULT="${RESULTS_DIR}/current.txt"
MAIN_RESULT="${RESULTS_DIR}/main.txt"
SIMBA_RESULT="${RESULTS_DIR}/simba.txt"
SUMMARY_RESULT="${RESULTS_DIR}/benchmark_summary.txt"

# ---------------------------------------------------------------------------
# Driver locations
# ---------------------------------------------------------------------------

DRIVER_PATH="${WORKSPACE_DIR}/cmake-out/google/cloud/odbc/libgoogle_cloud_odbc_bq_driver.so"

CURRENT_SO="${RESULTS_DIR}/libgoogle_cloud_odbc_bq_driver_current.so"
MAIN_SO="${RESULTS_DIR}/libgoogle_cloud_odbc_bq_driver_main.so"

CURRENT_SO_GCS="${PERF_DRIVER_BUCKET}/${SANITIZED_BRANCH}/libgoogle_cloud_odbc_bq_driver.so"
MAIN_SO_GCS="${PERF_DRIVER_BUCKET}/${SANITIZED_BRANCH}/libgoogle_cloud_odbc_bq_driver.so"

# ---------------------------------------------------------------------------
# ODBC configuration
# ---------------------------------------------------------------------------

GOOGLE_ODBCINI="/opt/odbc-driver/odbc.ini"
SIMBA_ODBCINI="/opt/odbc-driver/googlebigqueryodbc/odbc.ini"

export ODBC_TESTS_DSN="${ODBC_TESTS_DSN:-SampleDSNGoogleDriver}"

# ---------------------------------------------------------------------------
# Build performance_test
# ---------------------------------------------------------------------------

echo
echo "============================================================"
echo "Building performance_test"
echo "============================================================"

mapfile -t cmake_args < <(cmake::common_args)

io::run cmake \
  -S "${WORKSPACE_DIR}" \
  -B "${BUILD_DIR}" \
  "${cmake_args[@]}" \
  -DCMAKE_CXX_STANDARD=17 \
  -DBUILD_PERFORMANCE_TEST_ONLY=ON

io::run cmake \
  --build "${BUILD_DIR}" \
  --target performance_test \
  --parallel "$(nproc)"

PERFORMANCE_TEST="${BUILD_DIR}/integration_tests/performance_test"

if [[ ! -x "${PERFORMANCE_TEST}" ]]; then
  PERFORMANCE_TEST="${BUILD_DIR}/google/cloud/odbc/integration_tests/performance_test"
fi

if [[ ! -x "${PERFORMANCE_TEST}" ]]; then
  echo "ERROR: performance_test was not found."

  find "${BUILD_DIR}" \
    -type f \
    -name "performance_test" \
    -print 2>/dev/null || true

  exit 1
fi

echo "performance_test:"
echo "  ${PERFORMANCE_TEST}"

# ---------------------------------------------------------------------------
# Validate ODBC configuration
# ---------------------------------------------------------------------------

if [[ ! -f "${GOOGLE_ODBCINI}" ]]; then
  echo "ERROR: Google ODBC configuration not found:"
  echo "  ${GOOGLE_ODBCINI}"
  exit 1
fi

if [[ ! -f "${SIMBA_ODBCINI}" ]]; then
  echo "ERROR: Simba ODBC configuration not found:"
  echo "  ${SIMBA_ODBCINI}"
  exit 1
fi

# ---------------------------------------------------------------------------
# Download Google Current driver
# ---------------------------------------------------------------------------

echo
echo "============================================================"
echo "Downloading Google Current driver"
echo "============================================================"
echo "GCS:"
echo "  ${CURRENT_SO_GCS}"

gcloud storage cp \
  "${CURRENT_SO_GCS}" \
  "${CURRENT_SO}"

ls -lh "${CURRENT_SO}"

# ---------------------------------------------------------------------------
# Download Google Main driver
# ---------------------------------------------------------------------------

echo
echo "============================================================"
echo "Downloading Google Main driver"
echo "============================================================"
echo "GCS:"
echo "  ${MAIN_SO_GCS}"

gcloud storage cp \
  "${MAIN_SO_GCS}" \
  "${MAIN_SO}"

ls -lh "${MAIN_SO}"

# ---------------------------------------------------------------------------
# Run benchmark
# ---------------------------------------------------------------------------

run_benchmark() {
  local name="$1"
  local output_file="$2"
  local driver_so="$3"
  local dsn="$4"

  echo
  echo "============================================================"
  echo "Running benchmark: ${name}"
  echo "============================================================"
  echo "Driver:"
  echo "  ${driver_so}"
  echo "DSN:"
  echo "  ${dsn}"
  echo "Iterations:"
  echo "  ${BENCHMARK_ITERATIONS}"

  : > "${output_file}"

  local test_exit_code=0

  for i in $(seq 1 "${BENCHMARK_ITERATIONS}"); do
    echo
    echo "=== ${name}: iteration ${i}/${BENCHMARK_ITERATIONS} ==="

    echo "=== benchmark iteration ${i}/${BENCHMARK_ITERATIONS} ===" \
      >> "${output_file}"

    set +e

    ODBCINI="${dsn}" \
      ODBC_TESTS_DSN="${ODBC_TESTS_DSN}" \
      "${PERFORMANCE_TEST}" \
      >> "${output_file}" 2>&1

    run_exit=$?

    set -e

    if [[ "${run_exit}" -ne 0 ]]; then
      echo "WARNING: ${name} iteration ${i} failed with exit code ${run_exit}"
      test_exit_code="${run_exit}"
    fi
  done

  echo
  echo "Raw result:"
  echo "  ${output_file}"

  if [[ "${test_exit_code}" -ne 0 ]]; then
    echo
    echo "============================================================"
    echo "Benchmark failure output: ${name}"
    echo "============================================================"

    cat "${output_file}"

    echo
    echo "============================================================"

    return "${test_exit_code}"
  fi

  return 0
}

# ---------------------------------------------------------------------------
# Google Current
#
# The existing Google DSN points to DRIVER_PATH.
# Replace the driver binary before running the benchmark.
# ---------------------------------------------------------------------------

echo
echo "============================================================"
echo "Preparing Google Current"
echo "============================================================"

cp "${CURRENT_SO}" "${DRIVER_PATH}"

ls -lh "${DRIVER_PATH}"

export ODBC_TESTS_DSN="${ODBC_TESTS_DSN:-SampleDSNGoogleDriver}"

run_benchmark \
  "Google Current" \
  "${CURRENT_RESULT}" \
  "${DRIVER_PATH}" \
  "${GOOGLE_ODBCINI}"

# ---------------------------------------------------------------------------
# Google Main
# ---------------------------------------------------------------------------

echo
echo "============================================================"
echo "Preparing Google Main"
echo "============================================================"

cp "${MAIN_SO}" "${DRIVER_PATH}"

ls -lh "${DRIVER_PATH}"

run_benchmark \
  "Google Main" \
  "${MAIN_RESULT}" \
  "${DRIVER_PATH}" \
  "${GOOGLE_ODBCINI}"

# ---------------------------------------------------------------------------
# Simba
# ---------------------------------------------------------------------------

echo
echo "============================================================"
echo "Preparing Simba"
echo "============================================================"

export ODBC_TESTS_DSN="${SIMBA_ODBC_TESTS_DSN:-SampleDSN}"

run_benchmark \
  "Simba" \
  "${SIMBA_RESULT}" \
  "/opt/odbc-driver/googlebigqueryodbc" \
  "${SIMBA_ODBCINI}"

# ---------------------------------------------------------------------------
# Generate comparison
# ---------------------------------------------------------------------------

echo
echo "============================================================"
echo "Generating benchmark comparison"
echo "============================================================"

PARSER="${WORKSPACE_DIR}/ci/cloudbuild/builds/lib/benchmark_results.py"

if [[ ! -f "${PARSER}" ]]; then
  echo "ERROR: benchmark_results.py was not found:"
  echo "  ${PARSER}"
  exit 1
fi

python3 "${PARSER}" \
  --simba "${SIMBA_RESULT}" \
  --current "${CURRENT_RESULT}" \
  --main "${MAIN_RESULT}" \
  --output "${SUMMARY_RESULT}"

echo
echo "============================================================"
echo "Benchmark Summary"
echo "============================================================"

cat "${SUMMARY_RESULT}"

# ---------------------------------------------------------------------------
# Upload results
# ---------------------------------------------------------------------------

RESULTS_BUCKET="${PERF_DRIVER_BUCKET}/${SANITIZED_BRANCH}/benchmarks"

echo
echo "============================================================"
echo "Uploading benchmark results"
echo "============================================================"

gcloud storage cp \
  "${CURRENT_RESULT}" \
  "${MAIN_RESULT}" \
  "${SIMBA_RESULT}" \
  "${SUMMARY_RESULT}" \
  "${RESULTS_BUCKET}/"

echo
echo "Results uploaded to:"
echo "  ${RESULTS_BUCKET}/"

echo
echo "============================================================"
echo "Benchmark completed successfully"
echo "============================================================"