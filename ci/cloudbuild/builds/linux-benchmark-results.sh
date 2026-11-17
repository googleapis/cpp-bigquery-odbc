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
source module ci/lib/io.sh

WORKSPACE_DIR=$(pwd)

PERF_RESULTS_BUCKET="gs://bq-dev-tools-testing-drivers/odbc-perf"
RESULTS_DIR="${WORKSPACE_DIR}/benchmark-results"

rm -rf "${RESULTS_DIR}"
mkdir -p "${RESULTS_DIR}"

BRANCH_NAME="${BRANCH_NAME:-main}"

SANITIZED_BRANCH="$(
  echo "${BRANCH_NAME}" | sed -E 's/[^a-zA-Z0-9-]/_/g'
)"

RESULTS_BUCKET="${PERF_RESULTS_BUCKET}/${SANITIZED_BRANCH}/linux_results"
MAIN_RESULTS_BUCKET="${PERF_RESULTS_BUCKET}/main/linux_results"

CURRENT_RESULT="${RESULTS_DIR}/current_bq.txt"
EXISTING_RESULT="${RESULTS_DIR}/current_core.txt"
MAIN_RESULT="${RESULTS_DIR}/main_bq.txt"
SUMMARY_RESULT="${RESULTS_DIR}/benchmark_summary_table.txt"

echo "============================================================"
echo "Linux ODBC Benchmark Results Table Generator"
echo "============================================================"
echo "Branch       : ${BRANCH_NAME}"
echo "Results GCS  : ${RESULTS_BUCKET}"
echo

echo "Downloading benchmark results from GCS"
echo "------------------------------------------------------------"

gcloud storage cp \
  "${RESULTS_BUCKET}/performance_benchmark_results_BqDriver.txt" \
  "${CURRENT_RESULT}" || true

gcloud storage cp \
  "${RESULTS_BUCKET}/performance_benchmark_results_Core.txt" \
  "${EXISTING_RESULT}" || true

gcloud storage cp \
  "${MAIN_RESULTS_BUCKET}/performance_benchmark_results_BqDriver.txt" \
  "${MAIN_RESULT}" || true

echo
echo "Generating benchmark comparison table"
echo "------------------------------------------------------------"

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

echo
echo "Uploading benchmark summary table"
echo "------------------------------------------------------------"

gcloud storage cp \
  "${SUMMARY_RESULT}" \
  "${RESULTS_BUCKET}/"

echo
echo "Uploaded benchmark summary table to:"
echo "  ${RESULTS_BUCKET}/benchmark_summary_table.txt"
