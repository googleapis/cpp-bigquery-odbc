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
INTEGRATION_BUILDS_BUCKET="gs://odbc-integration-builds"
PERF_RESULTS_BUCKET="gs://bq-dev-tools-testing-drivers/odbc-perf"

BUILD_DIR="${WORKSPACE_DIR}/cmake-out"
RESULTS_DIR="${WORKSPACE_DIR}/benchmark-results"

rm -rf "${RESULTS_DIR}"
mkdir -p "${RESULTS_DIR}"

BRANCH_NAME="${BRANCH_NAME:-main}"

SANITIZED_BRANCH="$(
  echo "${BRANCH_NAME}" | sed -E 's/[^a-zA-Z0-9-]/_/g'
)"

# ============================================================================
# Result files
# ============================================================================

RESULTS_BUCKET="${PERF_RESULTS_BUCKET}/${SANITIZED_BRANCH}/linux_results"
MAIN_RESULTS_GCS="${PERF_RESULTS_BUCKET}/main/linux_results/performance_benchmark_results_BqDriver.txt"

CURRENT_RESULT="${RESULTS_DIR}/performance_benchmark_results_BqDriver.txt"
EXISTING_RESULT="${RESULTS_DIR}/performance_benchmark_results_Core.txt"
MAIN_RESULT="${RESULTS_DIR}/main_bq.txt"
SUMMARY_RESULT="${RESULTS_DIR}/benchmark_summary_table.txt"

# ============================================================================
# Driver locations
# ============================================================================

DRIVER_PATH="${BUILD_DIR}/google/cloud/odbc/libgoogle_cloud_odbc_bq_driver.so"

CURRENT_SO="${RESULTS_DIR}/libgoogle_cloud_odbc_bq_driver_current.so"
CURRENT_SO_GCS="${INTEGRATION_BUILDS_BUCKET}/${SANITIZED_BRANCH}/libgoogle_cloud_odbc_bq_driver.so"

# ============================================================================
# ODBC configuration
# ============================================================================

GOOGLE_ODBCINI="/opt/odbc-driver/odbc.ini"
EXISTING_ODBCINI="/opt/odbc-driver/googlebigqueryodbc/odbc.ini"

export ODBC_TESTS_DSN="SampleDSNGoogleDriver"
export CPP_BIGQUERY_ODBC_TEST_TABLE_PREFIX="${TRIGGER_NAME//[-:;.,?]/_}_${BRANCH_NAME//[-:;.,?]/_}"
export ODBCINSTINI="/opt/odbc-driver/odbcinst.ini"
export ODBCINI="${GOOGLE_ODBCINI}"

mapfile -t cmake_args < <(cmake::common_args)

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

# ============================================================================
# Header
# ============================================================================

echo "============================================================"
echo "Linux ODBC Performance Benchmark"
echo "============================================================"
echo "Branch       : ${BRANCH_NAME}"
echo "Iterations   : ${BENCHMARK_ITERATIONS}"
echo "Workspace    : ${WORKSPACE_DIR}"
echo

# ============================================================================
# Download drivers
# ============================================================================

echo "Downloading Google Current driver"
echo "------------------------------------------------------------"

if gcloud storage cp "${CURRENT_SO_GCS}" "${CURRENT_SO}"; then
  echo "Current Google driver downloaded successfully."
  HAS_CURRENT_DRIVER=true
else
  echo "WARNING: Current Google driver was not found at ${CURRENT_SO_GCS}."
  echo "WARNING: Google Current benchmark will be skipped."
  HAS_CURRENT_DRIVER=false
fi

mkdir -p "$(dirname "${DRIVER_PATH}")"

cp \
  /opt/odbc-driver/roots.pem \
  "${DRIVER_PATH%/*}/roots.pem"

# ============================================================================
# Run benchmark
# ============================================================================

run_benchmark() {
  local name="$1"
  local output_file="$2"
  local dsn="$3"
  local bq_tests_flag="$4"

  : >"${output_file}"

  echo
  echo "Reconfiguring performance_test for ${name}"
  echo "BQ_DRIVER_INTEGRATION_TESTS = ${bq_tests_flag}"
  echo "------------------------------------------------------------"

  if ! io::run cmake \
    -S "${WORKSPACE_DIR}" \
    -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_PERFORMANCE_TEST_ONLY=ON \
    -DBQ_DRIVER_INTEGRATION_TESTS="${bq_tests_flag}"; then
    echo "WARNING: CMake configuration failed for ${name}; skipping benchmark."
    return 0
  fi

  if ! io::run cmake \
    --build "${BUILD_DIR}" \
    --target performance_test \
    --parallel "$(nproc)"; then
    echo "WARNING: Building performance_test failed for ${name}; skipping benchmark."
    return 0
  fi

  local performance_test="${BUILD_DIR}/integration_tests/performance_test"

  if [[ ! -x "${performance_test}" ]]; then
    performance_test="${BUILD_DIR}/google/cloud/odbc/integration_tests/performance_test"
  fi

  if [[ ! -x "${performance_test}" ]]; then
    echo "WARNING: performance_test was not found for ${name}; skipping benchmark."

    find "${BUILD_DIR}" \
      -type f \
      -name "performance_test" \
      -print 2>/dev/null || true

    return 0
  fi

  echo "performance_test:"
  echo "  ${performance_test}"

  local failed_iterations=0

  for ((i = 1; i <= BENCHMARK_ITERATIONS; i++)); do
    echo
    echo "=== ${name}: iteration ${i}/${BENCHMARK_ITERATIONS} ==="

    echo "=== benchmark iteration ${i}/${BENCHMARK_ITERATIONS} ===" \
      >>"${output_file}"

    echo "=== ODBC tests DSN is====${ODBC_TESTS_DSN}==="
    echo "=== ODBCINI is====${dsn}==="

    set +e

    ODBCINI="${dsn}" \
      ODBC_TESTS_DSN="${ODBC_TESTS_DSN}" \
      "${performance_test}" \
      >>"${output_file}" 2>&1

    local run_exit=$?

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

      ((failed_iterations += 1))
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
# Google Current (BqDriver)
# ============================================================================

echo
echo "Preparing Google Current"
echo "------------------------------------------------------------"

if [[ "${HAS_CURRENT_DRIVER}" == "true" ]]; then
  cp "${CURRENT_SO}" "${DRIVER_PATH}"
  ls -lh "${DRIVER_PATH}"

  run_benchmark \
    "Google Current" \
    "${CURRENT_RESULT}" \
    "${GOOGLE_ODBCINI}" \
    "ON"

  echo
  echo "Uploading Google Current benchmark results"
  echo "------------------------------------------------------------"

  gcloud storage cp "${CURRENT_RESULT}" "${RESULTS_BUCKET}/" ||
    echo "WARNING: Failed to upload ${CURRENT_RESULT} to ${RESULTS_BUCKET}/"
else
  echo "Google Current benchmark skipped: current driver artifact unavailable."
  : >"${CURRENT_RESULT}"
fi

# ============================================================================
# Existing Driver (Core / Simba)
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

echo
echo "Uploading Existing Driver benchmark results"
echo "------------------------------------------------------------"

gcloud storage cp "${EXISTING_RESULT}" "${RESULTS_BUCKET}/" ||
  echo "WARNING: Failed to upload ${EXISTING_RESULT} to ${RESULTS_BUCKET}/"

# ============================================================================
# Download Main Results & Generate Comparison
# ============================================================================

echo
echo "Downloading Google Main benchmark results from GCS"
echo "------------------------------------------------------------"

gcloud storage cp "${MAIN_RESULTS_GCS}" "${MAIN_RESULT}" ||
  echo "WARNING: Main benchmark results not found at ${MAIN_RESULTS_GCS}; Google Main will be shown as N/A."

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
  --branch-name "${BRANCH_NAME}" \
  --commit-sha "${COMMIT_SHA:-$(git rev-parse HEAD)}" \
  --output "${SUMMARY_RESULT}"

# ============================================================================
# Upload Summary Table
# ============================================================================

echo
echo "Uploading benchmark summary table"
echo "------------------------------------------------------------"

gcloud storage cp \
  "${SUMMARY_RESULT}" \
  "${RESULTS_BUCKET}/"

echo
echo "Summary table uploaded to:"
echo "  ${RESULTS_BUCKET}/benchmark_summary_table.txt"

echo
echo "============================================================"
echo "Benchmark completed successfully"
echo "============================================================"
