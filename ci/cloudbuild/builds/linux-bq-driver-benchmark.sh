#!/bin/bash
#
# Copyright 2025 Google LLC
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
source module ci/cloudbuild/builds/lib/bazel.sh
source module ci/cloudbuild/builds/lib/secrets.sh
source module ci/cloudbuild/builds/lib/unit-tests.sh
source module ci/lib/io.sh

WORKSPACE_DIR=$(pwd)

# ------------------------------------------------------------
# Vcpkg
# ------------------------------------------------------------

VCPKG_VERSION=$(cat /tmp/vcpkg-version.txt)
export VCPKG_VERSION

echo "Using VCPKG_VERSION=${VCPKG_VERSION}"

export VCPKG_ROOT=/vcpkg

git clone --branch "${VCPKG_VERSION}" \
  https://github.com/microsoft/vcpkg.git \
  "${VCPKG_ROOT}"

cd "${VCPKG_ROOT}"
git checkout "${VCPKG_VERSION}"

./bootstrap-vcpkg.sh -disableMetrics

cd "${WORKSPACE_DIR}"

# ------------------------------------------------------------
# Unit tests
# ------------------------------------------------------------

mapfile -t args < <(bazel::common_args)
mapfile -t unit_tests_args < <(unit_tests::bazel_args)
mapfile -t secrets_bazel < <(secrets::bazel_args)

io::run bazel test \
  "${args[@]}" \
  "${secrets_bazel[@]}" \
  "${unit_tests_args[@]}" \
  --test_tag_filters=unit-tests \
  ...

# ------------------------------------------------------------
# Common CMake arguments
# ------------------------------------------------------------

mapfile -t cmake_args < <(cmake::common_args)

# ------------------------------------------------------------
# Benchmark configuration
#
# This build is ONLY for performance_test.
#
# We intentionally use a separate cmake-out directory so that
# this benchmark build does not interfere with the normal build.
# ------------------------------------------------------------

BUILD_DIR="${WORKSPACE_DIR}/cmake-out"

export ODBC_TESTS_DSN="SampleDSN"
export ODBC_TRANSACTIONS_TESTS_DSN="ODBCTransactionsTestsDSN"

export CPP_BIGQUERY_ODBC_TEST_TABLE_PREFIX="${TRIGGER_NAME//[-:;.,?]/_}_${BRANCH_NAME//[-:;.,?]/_}"

# ------------------------------------------------------------
# Check unixODBC
# ------------------------------------------------------------

if command -v odbcinst &>/dev/null; then
  export UNIXODBC_INSTALLED=true
  echo "unixODBC is installed."
else
  export UNIXODBC_INSTALLED=false
  export ODBCINSTINI=/opt/odbc-driver/odbcinst.ini
  echo "unixODBC is not installed."
fi

# ------------------------------------------------------------
# Configure Google Driver + performance_test
# ------------------------------------------------------------

echo "============================================================"
echo "Configuring Google ODBC performance benchmark build"
echo "Build directory: ${BUILD_DIR}"
echo "============================================================"

io::run cmake -S "${WORKSPACE_DIR}" -B "${BUILD_DIR}" \
  "${cmake_args[@]}" \
  -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" \
  -DCMAKE_CXX_STANDARD=17 \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_PERFORMANCE_TEST_ONLY=ON \
  -DBQ_DRIVER_INTEGRATION_TESTS=ON

# ------------------------------------------------------------
# Build ONLY performance_test
# ------------------------------------------------------------

echo "============================================================"
echo "Building Google Driver performance_test"
echo "============================================================"

io::run cmake --build "${BUILD_DIR}" \
  --target performance_test \
  --config Release \
  --parallel "$(nproc)"

# ------------------------------------------------------------
# Locate performance_test
# ------------------------------------------------------------

PERFORMANCE_TEST="${BUILD_DIR}/google/cloud/odbc/integration_tests/performance_test"

if [[ ! -f "${PERFORMANCE_TEST}" ]]; then
  echo "ERROR: performance_test was not found:"
  echo "  ${PERFORMANCE_TEST}"
  echo
  echo "Build directory contents:"
  find "${BUILD_DIR}" -type f -name "performance_test*" -print
  exit 1
fi

echo "Google performance_test:"
echo "  ${PERFORMANCE_TEST}"

# ------------------------------------------------------------
# Copy certificates required by Google Driver
# ------------------------------------------------------------

GOOGLE_DRIVER_DIR="${BUILD_DIR}/google/cloud/odbc"

if [[ -f /opt/odbc-driver/roots.pem ]]; then
  io::run cp \
    /opt/odbc-driver/roots.pem \
    "${GOOGLE_DRIVER_DIR}/roots.pem"
else
  echo "ERROR: /opt/odbc-driver/roots.pem not found"
  exit 1
fi

# ------------------------------------------------------------
# Google Driver benchmark DSN
# ------------------------------------------------------------

export ODBCINI=/opt/odbc-driver/odbc.ini

echo "Using ODBCINI=${ODBCINI}"
echo "Using Google DSN=${ODBC_TESTS_DSN}"

# Verify Google driver exists.
GOOGLE_DRIVER="${BUILD_DIR}/google/cloud/odbc/libgoogle_cloud_odbc_bq_driver.so"

if [[ ! -f "${GOOGLE_DRIVER}" ]]; then
  echo "ERROR: Google driver not found:"
  echo "  ${GOOGLE_DRIVER}"
  exit 1
fi

echo "Google driver:"
echo "  ${GOOGLE_DRIVER}"

# ------------------------------------------------------------
# Run Google Driver benchmark
#
# One complete performance_test suite run.
#
# BENCHMARK_ITERATIONS can be supplied by the CI trigger.
# Default = 1.
#
# Every test in performance_test is compared, not just one test.
# ------------------------------------------------------------

BENCHMARK_ITERATIONS="${BENCHMARK_ITERATIONS:-1}"

GOOGLE_RESULTS="${WORKSPACE_DIR}/benchmark_results_current_bq.txt"

echo "============================================================"
echo "Running Google Driver performance suite"
echo "Iterations: ${BENCHMARK_ITERATIONS}"
echo "============================================================"

: > "${GOOGLE_RESULTS}"

GOOGLE_EXIT_CODE=0

for ((i = 1; i <= BENCHMARK_ITERATIONS; i++)); do
  echo "=== Google Driver benchmark iteration ${i}/${BENCHMARK_ITERATIONS} ===" \
    >> "${GOOGLE_RESULTS}"

  set +e
  "${PERFORMANCE_TEST}" >> "${GOOGLE_RESULTS}" 2>&1
  RUN_EXIT_CODE=$?
  set -e

  if [[ ${RUN_EXIT_CODE} -ne 0 ]]; then
    echo "WARNING: Google Driver iteration ${i} failed with ${RUN_EXIT_CODE}"
    GOOGLE_EXIT_CODE=${RUN_EXIT_CODE}
  fi
done

if [[ ${GOOGLE_EXIT_CODE} -ne 0 ]]; then
  echo "ERROR: Google Driver performance_test failed."
  exit "${GOOGLE_EXIT_CODE}"
fi

# ------------------------------------------------------------
# Run Simba Driver performance suite
#
# Simba is already installed/configured by the Simba pipeline.
#
# We do NOT rebuild or download Simba here.
# ------------------------------------------------------------

export ODBC_TESTS_DSN="SampleDSN"

SIMBA_RESULTS="${WORKSPACE_DIR}/benchmark_results_current_core.txt"

echo "============================================================"
echo "Running Simba Driver performance suite"
echo "DSN: ${ODBC_TESTS_DSN}"
echo "============================================================"

: > "${SIMBA_RESULTS}"

SIMBA_EXIT_CODE=0

for ((i = 1; i <= BENCHMARK_ITERATIONS; i++)); do
  echo "=== Simba Driver benchmark iteration ${i}/${BENCHMARK_ITERATIONS} ===" \
    >> "${SIMBA_RESULTS}"

  set +e
  "${PERFORMANCE_TEST}" >> "${SIMBA_RESULTS}" 2>&1
  RUN_EXIT_CODE=$?
  set -e

  if [[ ${RUN_EXIT_CODE} -ne 0 ]]; then
    echo "WARNING: Simba Driver iteration ${i} failed with ${RUN_EXIT_CODE}"
    SIMBA_EXIT_CODE=${RUN_EXIT_CODE}
  fi
done

if [[ ${SIMBA_EXIT_CODE} -ne 0 ]]; then
  echo "ERROR: Simba Driver performance_test failed."
  exit "${SIMBA_EXIT_CODE}"
fi

# ------------------------------------------------------------
# Main branch Google benchmark
#
# Main is optional. If it has already been uploaded, download it
# and include it in the comparison.
# ------------------------------------------------------------

mkdir -p "${WORKSPACE_DIR}/benchmark_results"

CURRENT_BRANCH="${BRANCH_NAME:-main}"

SANITIZED_BRANCH=$(echo "${CURRENT_BRANCH}" | \
  sed 's/[^a-zA-Z0-9._-]/_/g')

MAIN_RESULTS="${WORKSPACE_DIR}/benchmark_results/main_bq.txt"

echo "============================================================"
echo "Downloading main branch Google benchmark baseline"
echo "============================================================"

set +e

gcloud storage cp \
  "gs://bq-dev-tools-testing-drivers/odbc-perf/main/results/performance_benchmark_results_BqDriver.txt" \
  "${MAIN_RESULTS}"

MAIN_DOWNLOAD_EXIT=$?

set -e

if [[ ${MAIN_DOWNLOAD_EXIT} -ne 0 ]]; then
  echo "WARNING: Main branch benchmark result not available."
  rm -f "${MAIN_RESULTS}"
fi

# ------------------------------------------------------------
# Parse results and generate comparison table
# ------------------------------------------------------------

SUMMARY_FILE="${WORKSPACE_DIR}/benchmark_summary_table.txt"

python3 <<'PYTHON'
import os
import re

SIMBA_FILE = "benchmark_results_current_core.txt"
GOOGLE_FILE = "benchmark_results_current_bq.txt"
MAIN_FILE = "benchmark_results/main_bq.txt"
OUTPUT_FILE = "benchmark_summary_table.txt"


def clean_test_name(name):
    # GTest:
    #
    # [Instantiation/]TestSuite.TestCase[/Param]
    #
    # Keep TestCase so that the same test can be compared
    # between Simba and Google drivers.

    if "." in name:
        name = name.split(".", 1)[1]

    # Remove old HTAPI parameter suffix if present.
    name = re.sub(r"/(?:With|Without)HTAPI$", "", name)

    return name


def parse_time_to_ms(value):
    if not value:
        return None

    value = value.strip()

    if value == "N/A":
        return None

    match = re.match(r"^([\d.]+)\s*(\w+)$", value)

    if not match:
        return None

    number = float(match.group(1))
    unit = match.group(2).lower()

    if unit == "ms":
        return number

    if unit == "s":
        return number * 1000

    if unit == "us":
        return number / 1000

    if unit == "ns":
        return number / 1_000_000

    return number


def parse_gtest_output(filename):
    samples = {}

    if not os.path.exists(filename):
        return {}

    pattern = re.compile(
        r"\[\s+OK\s+\]\s+(\S+)\s+\(([^)]+)\)"
    )

    with open(filename, "r", errors="replace") as file:
        for line in file:
            match = pattern.search(line)

            if not match:
                continue

            test_name = clean_test_name(match.group(1))
            duration = parse_time_to_ms(match.group(2))

            if duration is None:
                continue

            samples.setdefault(test_name, []).append(duration)

    # Median per test across iterations.
    results = {}

    for test_name, values in samples.items():
        values.sort()

        n = len(values)

        if n % 2:
            median = values[n // 2]
        else:
            median = (
                values[n // 2 - 1] +
                values[n // 2]
            ) / 2.0

        results[test_name] = median

    return results


def percentage(value, reference):
    if value is None or reference is None or reference == 0:
        return "N/A"

    pct = ((value - reference) / reference) * 100

    return f"{pct:+.0f}%"


simba = parse_gtest_output(SIMBA_FILE)
google = parse_gtest_output(GOOGLE_FILE)
main_google = parse_gtest_output(MAIN_FILE)

all_tests = sorted(
    set(simba.keys()) |
    set(google.keys()) |
    set(main_google.keys())
)

rows = []

for test in all_tests:

    simba_ms = simba.get(test)
    google_ms = google.get(test)
    main_ms = main_google.get(test)

    google_vs_simba = percentage(
        google_ms,
        simba_ms
    )

    main_vs_google = percentage(
        main_ms,
        google_ms
    )

    simba_value = (
        f"{simba_ms:.0f}ms"
        if simba_ms is not None
        else "N/A"
    )

    google_value = (
        f"{google_ms:.0f}ms ({google_vs_simba})"
        if google_ms is not None
        else "N/A"
    )

    main_value = (
        f"{main_ms:.0f}ms ({main_vs_google})"
        if main_ms is not None
        else "N/A"
    )

    rows.append(
        (
            test,
            simba_value,
            google_value,
            main_value,
        )
    )


headers = [
    "Test Case",
    "Simba Driver",
    "Google Driver (Current)",
    "Google Driver (Main)",
]

widths = []

for index, header in enumerate(headers):
    width = len(header)

    for row in rows:
        width = max(width, len(row[index]))

    widths.append(width)


table = []

table.append(
    "| "
    + " | ".join(
        header.ljust(widths[index])
        for index, header in enumerate(headers)
    )
    + " |"
)

table.append(
    "| "
    + " | ".join(
        "-" * widths[index]
        for index in range(len(headers))
    )
    + " |"
)

for row in rows:
    table.append(
        "| "
        + " | ".join(
            row[index].ljust(widths[index])
            for index in range(len(headers))
        )
        + " |"
    )


description = (
    "Percentages in **Google Driver (Current)** are relative to "
    "**Simba Driver**. Percentages in **Google Driver (Main)** "
    "are relative to **Google Driver (Current)**. "
    "Negative values mean the Google Driver is faster; "
    "positive values mean it is slower."
)

output = description + "\n\n" + "\n".join(table) + "\n"

with open(OUTPUT_FILE, "w") as file:
    file.write(output)

print(output)
print(f"Compared {len(all_tests)} performance tests.")

PYTHON

# ------------------------------------------------------------
# Upload current results
# ------------------------------------------------------------

RESULTS_BUCKET="gs://bq-dev-tools-testing-drivers/odbc-perf/${SANITIZED_BRANCH}/results"

echo "============================================================"
echo "Uploading benchmark results"
echo "Destination: ${RESULTS_BUCKET}"
echo "============================================================"

io::run gcloud storage cp \
  "${GOOGLE_RESULTS}" \
  "${RESULTS_BUCKET}/performance_benchmark_results_BqDriver.txt"

io::run gcloud storage cp \
  "${SIMBA_RESULTS}" \
  "${RESULTS_BUCKET}/performance_benchmark_results_Core.txt"

io::run gcloud storage cp \
  "${SUMMARY_FILE}" \
  "${RESULTS_BUCKET}/benchmark_summary_table.txt"

echo
echo "============================================================"
echo "Benchmark completed"
echo "============================================================"
echo
echo "Simba results:"
echo "  ${SIMBA_RESULTS}"
echo
echo "Google results:"
echo "  ${GOOGLE_RESULTS}"
echo
echo "Comparison:"
echo "  ${SUMMARY_FILE}"
echo
echo "GCS:"
echo "  ${RESULTS_BUCKET}/"
echo "============================================================"