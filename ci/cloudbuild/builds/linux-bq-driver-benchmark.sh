#!/bin/bash
#
# Copyright 2026 Google LLC
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

set -euo pipefail

source "$(dirname "$0")/../../lib/init.sh"
source module ci/install-dependencies.sh

source module ci/cloudbuild/builds/lib/cmake.sh
source module ci/cloudbuild/builds/lib/secrets.sh
source module ci/lib/io.sh

WORKSPACE_DIR=$(pwd)

# ============================================================================
# Configuration
# ============================================================================

BENCHMARK_ITERATIONS="${BENCHMARK_ITERATIONS:-3}"

PERF_DRIVER_BUCKET="gs://bq-dev-tools-testing-drivers/odbc-perf"

BUILD_DIR="${WORKSPACE_DIR}/cmake-out"
RESULTS_DIR="${WORKSPACE_DIR}/benchmark-results"

rm -rf "${BUILD_DIR}"
rm -rf "${RESULTS_DIR}"

mkdir -p "${RESULTS_DIR}"

# ============================================================================
# Branch
# ============================================================================

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

# ============================================================================
# Result files
# ============================================================================

CURRENT_RESULT="${RESULTS_DIR}/current.txt"
MAIN_RESULT="${RESULTS_DIR}/main.txt"
EXISTING_RESULT="${RESULTS_DIR}/existing.txt"
SUMMARY_RESULT="${RESULTS_DIR}/benchmark_summary_linux.txt"

# ============================================================================
# Driver locations
# ============================================================================

DRIVER_PATH="${WORKSPACE_DIR}/cmake-out/google/cloud/odbc/libgoogle_cloud_odbc_bq_driver.so"

CURRENT_SO="${RESULTS_DIR}/libgoogle_cloud_odbc_bq_driver_current.so"
MAIN_SO="${RESULTS_DIR}/libgoogle_cloud_odbc_bq_driver_main.so"

CURRENT_SO_GCS="${PERF_DRIVER_BUCKET}/${SANITIZED_BRANCH}/linux/libgoogle_cloud_odbc_bq_driver.so"
MAIN_SO_GCS="${PERF_DRIVER_BUCKET}/main/linux/libgoogle_cloud_odbc_bq_driver.so"

# ============================================================================
# ODBC configuration
# ============================================================================

GOOGLE_ODBCINI="/opt/odbc-driver/odbc.ini"
EXISTING_ODBCINI="/opt/odbc-driver/googlebigqueryodbc/odbc.ini"

cd "$WORKSPACE_DIR"

# This is the name of DSN set in odbc.ini.
mapfile -t cmake_args < <(cmake::common_args)

GOOGLE_ODBCINI="/opt/odbc-driver/odbc.ini"
EXISTING_ODBCINI="/opt/odbc-driver/googlebigqueryodbc/odbc.ini"

export ODBC_TESTS_DSN="SampleDSNGoogleDriver"

export CPP_BIGQUERY_ODBC_TEST_TABLE_PREFIX=${TRIGGER_NAME//[-:;.,?]/_}_${BRANCH_NAME//[-:;.,?]/_}
export ODBCINSTINI=/opt/odbc-driver/odbcinst.ini
export ODBCINI="${GOOGLE_ODBCINI}"

# ============================================================================
# Validate ODBC configuration
# ============================================================================

if [[ ! -f "${GOOGLE_ODBCINI}" ]]; then
  echo "ERROR: Google ODBC configuration not found:"
  echo "  ${GOOGLE_ODBCINI}"
  exit 1
fi

if [[ ! -f "${EXISTING_ODBCINI}" ]]; then
  echo "ERROR: Existing ODBC configuration not found:"
  echo "  ${EXISTING_ODBCINI}"
  exit 1
fi

# ---------------------------------------------------------------------------
# Download Google Current driver
# ---------------------------------------------------------------------------

echo
echo "Downloading Google Current driver"
echo "------------------------------------------------------------"

gcloud storage cp \
  "${CURRENT_SO_GCS}" \
  "${CURRENT_SO}"

# ---------------------------------------------------------------------------
# Download Google Main driver
# ---------------------------------------------------------------------------

echo
echo "Downloading Google driver from main"
echo "------------------------------------------------------------"

if gcloud storage cp "$MAIN_SO_GCS" "$MAIN_SO"; then
  echo "Main Google driver downloaded successfully."
  HAS_MAIN_DRIVER=true
else
  echo "WARNING: Main Google driver was not found."
  echo "WARNING: Google Main benchmark will be skipped."
  HAS_MAIN_DRIVER=false
fi

# ============================================================================
# Run benchmark
# ============================================================================

run_benchmark() {
  local name="$1"
  local output_file="$2"
  local dsn="$3"
  local bq_tests_flag="$4"

  echo
  echo "Reconfiguring performance_test for ${name}"
  echo "BQ_DRIVER_INTEGRATION_TESTS = ${bq_tests_flag}"
  echo "------------------------------------------------------------"

  io::run cmake -S "${WORKSPACE_DIR}" \
    -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_PERFORMANCE_TEST_ONLY=ON \
    -DBQ_DRIVER_INTEGRATION_TESTS="${bq_tests_flag}"

  io::run cmake --build "${BUILD_DIR}" \
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

    return 1
  fi

  echo "performance_test:"
  echo "  ${PERFORMANCE_TEST}"

  : >"${output_file}"

  local failed_iterations=0

  for i in $(seq 1 "${BENCHMARK_ITERATIONS}"); do
    echo
    echo "=== ${name}: iteration ${i}/${BENCHMARK_ITERATIONS} ==="

    echo "=== benchmark iteration ${i}/${BENCHMARK_ITERATIONS} ===" \
      >>"${output_file}"

    echo "=== ODBC tests DSN is====${ODBC_TESTS_DSN}==="
    echo "=== ODBCINI is====${dsn}==="

    set +e

    ODBCINI="${dsn}" \
      ODBC_TESTS_DSN="${ODBC_TESTS_DSN}" \
      "${PERFORMANCE_TEST}" \
      >>"${output_file}" 2>&1

    run_exit=$?

    set -e

    if [[ "${run_exit}" -ne 0 ]]; then
      echo "WARNING: ${name} iteration ${i} failed with exit code ${run_exit}"
      echo "WARNING: Continuing with remaining iterations."

      echo "============================================================"
      echo "Detailed failure for ${name}, iteration ${i}:"
      echo "============================================================"

      grep -E -B 20 -A 20 \
        '\[ *FAILED *\]|Failure|FAILED|ERROR|Error|error|SQLSTATE|Diagnostic|diagnostic|SQL_ERROR|SQLConnect|SQLDriverConnect|Exception|exception' \
        "${output_file}" || true

      echo "============================================================"

      failed_iterations=$((failed_iterations + 1))
    fi
  done

  echo
  echo "${name} benchmark completed."
  echo "Failed iterations: ${failed_iterations}/${BENCHMARK_ITERATIONS}"
  echo "Raw result: ${output_file}"

  # Benchmark failures must not fail Cloud Build.
  return 0
}

# ============================================================================
# Google Current
#
# The existing Google DSN points to DRIVER_PATH.
# Replace the driver binary before running the benchmark.
# ============================================================================

cp \
  /opt/odbc-driver/roots.pem \
  "${WORKSPACE_DIR}/cmake-out/google/cloud/odbc/roots.pem"

echo
echo "Preparing Google Current"
echo "------------------------------------------------------------"

cp "${CURRENT_SO}" "${DRIVER_PATH}"
ls -lh "${DRIVER_PATH}"

run_benchmark \
  "Google Current" \
  "${CURRENT_RESULT}" \
  "${GOOGLE_ODBCINI}" \
  "ON"

# ============================================================================
# Google Main
# ============================================================================

echo
echo "Preparing Google Main"
echo "------------------------------------------------------------"

if [[ "$HAS_MAIN_DRIVER" == "true" ]]; then
  cp "${MAIN_SO}" "${DRIVER_PATH}"
  ls -lh "${DRIVER_PATH}"

  run_benchmark \
    "Google Main" \
    "${MAIN_RESULT}" \
    "${GOOGLE_ODBCINI}" \
    "ON"
else
  echo "Google Main benchmark skipped: main driver artifact unavailable."
  : >"$MAIN_RESULT"
fi

# ============================================================================
# Existing
# ============================================================================

echo
echo "Preparing Existing Driver"
echo "------------------------------------------------------------"

export ODBC_TESTS_DSN="SampleDSN"
export ODBCINI="${EXISTING_ODBCINI}"
export ODBCINSTINI="/opt/odbc-driver/googlebigqueryodbc/odbcinst.ini"

run_benchmark \
  "Existing" \
  "${EXISTING_RESULT}" \
  "${EXISTING_ODBCINI}" \
  "OFF"

# ============================================================================
# Generate comparison
# ============================================================================

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
  --existing "${EXISTING_RESULT}" \
  --current "${CURRENT_RESULT}" \
  --main "${MAIN_RESULT}" \
  --output "${SUMMARY_RESULT}"

# ============================================================================
# Upload results
# ============================================================================

RESULTS_BUCKET="${PERF_DRIVER_BUCKET}/${SANITIZED_BRANCH}/linux/results"

echo
echo "Uploading benchmark results"
echo "------------------------------------------------------------"

gcloud storage cp \
  "${CURRENT_RESULT}" \
  "${MAIN_RESULT}" \
  "${EXISTING_RESULT}" \
  "${SUMMARY_RESULT}" \
  "${RESULTS_BUCKET}/"

echo
echo "Results uploaded to:"
echo "  ${RESULTS_BUCKET}/"

echo
echo "============================================================"
echo "Benchmark completed successfully"
echo "============================================================"
