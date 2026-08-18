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

WORKSPACE_DIR=$(pwd)

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

BENCHMARK_ITERATIONS="${BENCHMARK_ITERATIONS:-3}"

PERF_DRIVER_BUCKET="gs://bq-dev-tools-testing-drivers/odbc-perf-drivers"

# performance_test is built by the current source tree.
BUILD_DIR="/opt/odbc-driver"

# ---------------------------------------------------------------------------
# Build performance_test
# ---------------------------------------------------------------------------

echo
echo "============================================================"
echo "Building performance_test"
echo "============================================================"

mapfile -t cmake_args < <(cmake::common_args)

io::run cmake \
  -S "$WORKSPACE_DIR" \
  -B "$BUILD_DIR" \
  "${cmake_args[@]}" \
  -DCMAKE_CXX_STANDARD=17 \
  -DBUILD_PERFORMANCE_TEST_ONLY=ON

io::run cmake \
  --build "$BUILD_DIR" \
  --target performance_test \
  --parallel "$(nproc)"

PERFORMANCE_TEST="${BUILD_DIR}/integration_tests/performance_test"

# Depending on the CMake layout, use the binary generated in cmake-out
# if the above path does not exist.
if [[ ! -x "$PERFORMANCE_TEST" ]]; then
  PERFORMANCE_TEST="${WORKSPACE_DIR}/cmake-out/integration_tests/performance_test"
fi

if [[ ! -x "$PERFORMANCE_TEST" ]]; then
  PERFORMANCE_TEST="${WORKSPACE_DIR}/cmake-out/google/cloud/odbc/integration_tests/performance_test"
fi

echo "============================================================"
echo "ODBC Performance Benchmark"
echo "============================================================"
echo "Branch              : ${BRANCH_NAME}"
echo "Iterations          : ${BENCHMARK_ITERATIONS}"
echo "Workspace            : ${WORKSPACE_DIR}"
echo

# ---------------------------------------------------------------------------
# Sanitize branch name
# ---------------------------------------------------------------------------

SANITIZED_BRANCH=$(
  echo "${BRANCH_NAME}" |
    sed -E 's/[^a-zA-Z0-9._-]/_/g'
)

echo "Sanitized branch: ${SANITIZED_BRANCH}"

# ---------------------------------------------------------------------------
# Temporary benchmark directory
# ---------------------------------------------------------------------------

RESULTS_DIR="${WORKSPACE_DIR}/benchmark-results"

rm -rf "$RESULTS_DIR"
mkdir -p "$RESULTS_DIR"

CURRENT_RESULT="${RESULTS_DIR}/current.txt"
MAIN_RESULT="${RESULTS_DIR}/main.txt"
SIMBA_RESULT="${RESULTS_DIR}/simba.txt"
SUMMARY_RESULT="${RESULTS_DIR}/benchmark_summary.txt"

# ---------------------------------------------------------------------------
# Google driver .so locations
# ---------------------------------------------------------------------------

CURRENT_SO="${RESULTS_DIR}/libgoogle_cloud_odbc_bq_driver_current.so"
MAIN_SO="${RESULTS_DIR}/libgoogle_cloud_odbc_bq_driver_main.so"

CURRENT_SO_GCS="${PERF_DRIVER_BUCKET}/${SANITIZED_BRANCH}/libgoogle_cloud_odbc_bq_driver.so"
MAIN_SO_GCS="${PERF_DRIVER_BUCKET}/${SANITIZED_BRANCH}/libgoogle_cloud_odbc_bq_driver.so"

# ---------------------------------------------------------------------------
# Simba configuration
#
# The Simba dependency setup already installs:
#
#   /opt/odbc-driver/googlebigqueryodbc/odbc.ini
#
# and sets:
#
#   ODBCINI=/opt/odbc-driver/googlebigqueryodbc/odbc.ini
#
# We intentionally do not create another DSN.
# ---------------------------------------------------------------------------

SIMBA_ODBCINI="/opt/odbc-driver/googlebigqueryodbc/odbc.ini"

if [[ ! -f "$SIMBA_ODBCINI" ]]; then
  echo "ERROR: Simba odbc.ini was not found:"
  echo "       ${SIMBA_ODBCINI}"
  exit 1
fi

# ---------------------------------------------------------------------------
# Download current Google driver
# ---------------------------------------------------------------------------

echo
echo "============================================================"
echo "Downloading Google driver for current branch"
echo "============================================================"

echo "GCS:"
echo "  ${CURRENT_SO_GCS}"

gcloud storage cp \
  "$CURRENT_SO_GCS" \
  "$CURRENT_SO"

if [[ ! -f "$CURRENT_SO" ]]; then
  echo "ERROR: Current Google driver was not downloaded."
  exit 1
fi

ls -lh "$CURRENT_SO"

# ---------------------------------------------------------------------------
# Download Google driver from main
# ---------------------------------------------------------------------------

echo
echo "============================================================"
echo "Downloading Google driver from main"
echo "============================================================"

echo "GCS:"
echo "  ${MAIN_SO_GCS}"

gcloud storage cp \
  "$MAIN_SO_GCS" \
  "$MAIN_SO"

if [[ ! -f "$MAIN_SO" ]]; then
  echo "ERROR: Main Google driver was not downloaded."
  exit 1
fi

ls -lh "$MAIN_SO"

# ---------------------------------------------------------------------------
# Locate performance_test
# ---------------------------------------------------------------------------

echo
echo "============================================================"
echo "Locating performance_test"
echo "============================================================"

if [[ ! -x "$PERFORMANCE_TEST" ]]; then
  echo "ERROR: performance_test executable was not found."

  echo
  echo "Searching cmake-out:"
  find "${WORKSPACE_DIR}/cmake-out" \
    -type f \
    -name "performance_test" \
    -print 2>/dev/null || true

  exit 1
fi

echo "performance_test:"
echo "  ${PERFORMANCE_TEST}"

# ---------------------------------------------------------------------------
# Helper: run benchmark
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

  : > "$output_file"

  local test_exit_code=0

  for i in $(seq 1 "${BENCHMARK_ITERATIONS}"); do
    echo
    echo "=== ${name}: iteration ${i}/${BENCHMARK_ITERATIONS} ==="

    echo "=== benchmark iteration ${i}/${BENCHMARK_ITERATIONS} ===" \
      >> "$output_file"

    set +e

    ODBCINI="$dsn" \
      ODBC_TESTS_DSN="${ODBC_TESTS_DSN}" \
      "$PERFORMANCE_TEST" \
      >> "$output_file" 2>&1

    run_exit=$?

    set -e

    if [[ "$run_exit" -ne 0 ]]; then
      echo "WARNING: ${name} iteration ${i} failed with exit code ${run_exit}"
      test_exit_code="$run_exit"
    fi
  done

  echo
  echo "Raw result:"
  echo "  ${output_file}"

  if [[ "$test_exit_code" -ne 0 ]]; then
    echo "ERROR: ${name} benchmark failed."
    return "$test_exit_code"
  fi

  return 0
}

# ---------------------------------------------------------------------------
# 1. Google Current
#
# Reuse the existing Google DSN.
# We don't create a new DSN.
#
# The only thing changed is the driver .so referenced by the DSN.
# ---------------------------------------------------------------------------

echo
echo "============================================================"
echo "Preparing Google Current benchmark"
echo "============================================================"

# Backup the existing odbc.ini because it contains the current Google DSN.
GOOGLE_ODBCINI="${RESULTS_DIR}/google_odbc.ini"

if [[ -n "${ODBCINI:-}" && -f "${ODBCINI}" ]]; then
  cp "${ODBCINI}" "$GOOGLE_ODBCINI"
else
  GOOGLE_ODBCINI="${WORKSPACE_DIR}/odbc.ini"

  if [[ ! -f "$GOOGLE_ODBCINI" ]]; then
    echo "ERROR: Google driver odbc.ini was not found."
    echo "ODBCINI=${ODBCINI:-<not set>}"
    exit 1
  fi
fi

echo "Google ODBC configuration:"
echo "  ${GOOGLE_ODBCINI}"

# ---------------------------------------------------------------------------
# IMPORTANT:
# Replace only the Driver= line in the existing Google DSN.
#
# We are NOT creating another DSN.
# ---------------------------------------------------------------------------

GOOGLE_CURRENT_ODBCINI="${RESULTS_DIR}/google_current_odbc.ini"

sed \
  "s|^Driver=.*|Driver=${CURRENT_SO}|" \
  "$GOOGLE_ODBCINI" \
  > "$GOOGLE_CURRENT_ODBCINI"

export ODBC_TESTS_DSN="${ODBC_TESTS_DSN:-SampleDSNGoogleDriver}"

run_benchmark \
  "Google Current" \
  "$CURRENT_RESULT" \
  "$CURRENT_SO" \
  "$GOOGLE_CURRENT_ODBCINI"

# ---------------------------------------------------------------------------
# 2. Google Main
# ---------------------------------------------------------------------------

echo
echo "============================================================"
echo "Preparing Google Main benchmark"
echo "============================================================"

GOOGLE_MAIN_ODBCINI="${RESULTS_DIR}/google_main_odbc.ini"

sed \
  "s|^Driver=.*|Driver=${MAIN_SO}|" \
  "$GOOGLE_ODBCINI" \
  > "$GOOGLE_MAIN_ODBCINI"

run_benchmark \
  "Google Main" \
  "$MAIN_RESULT" \
  "$MAIN_SO" \
  "$GOOGLE_MAIN_ODBCINI"

# ---------------------------------------------------------------------------
# 3. Simba
#
# Simba's existing dependency setup already provides:
#
#   /opt/odbc-driver/googlebigqueryodbc/odbc.ini
#
# Do not create another DSN.
# ---------------------------------------------------------------------------

echo
echo "============================================================"
echo "Preparing Simba benchmark"
echo "============================================================"

export ODBC_TESTS_DSN="${SIMBA_ODBC_TESTS_DSN:-SampleDSN}"

run_benchmark \
  "Simba" \
  "$SIMBA_RESULT" \
  "/opt/odbc-driver/googlebigqueryodbc" \
  "$SIMBA_ODBCINI"

# ---------------------------------------------------------------------------
# Parse results using EXISTING benchmark_results.py
# ---------------------------------------------------------------------------

echo
echo "============================================================"
echo "Generating benchmark comparison"
echo "============================================================"

PARSER="${WORKSPACE_DIR}/ci/cloudbuild/scripts/benchmark_results.py"

if [[ ! -f "$PARSER" ]]; then
  echo "ERROR: Existing benchmark_results.py was not found:"
  echo "       ${PARSER}"
  exit 1
fi

python3 "$PARSER" \
  --simba "$SIMBA_RESULT" \
  --current "$CURRENT_RESULT" \
  --main "$MAIN_RESULT" \
  --output "$SUMMARY_RESULT"

echo
echo "============================================================"
echo "Benchmark Summary"
echo "============================================================"

cat "$SUMMARY_RESULT"

# ---------------------------------------------------------------------------
# Upload raw results + summary
# ---------------------------------------------------------------------------

RESULTS_BUCKET="${PERF_DRIVER_BUCKET}/${SANITIZED_BRANCH}/benchmarks"

echo
echo "============================================================"
echo "Uploading benchmark results"
echo "============================================================"

gcloud storage cp \
  "$CURRENT_RESULT" \
  "$MAIN_RESULT" \
  "$SIMBA_RESULT" \
  "$SUMMARY_RESULT" \
  "${RESULTS_BUCKET}/"

echo
echo "Benchmark results uploaded to:"
echo "  ${RESULTS_BUCKET}/"

echo
echo "============================================================"
echo "Benchmark completed successfully"
echo "============================================================"