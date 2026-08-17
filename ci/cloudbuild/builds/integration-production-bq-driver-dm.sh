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
source module ci/lib/io.sh

WORKSPACE_DIR="$(pwd)"

# ============================================================
# VCPKG
# ============================================================

VCPKG_VERSION="$(cat /tmp/vcpkg-version.txt)"
export VCPKG_VERSION

echo "============================================================"
echo "VCPKG"
echo "============================================================"
echo "Using VCPKG_VERSION=${VCPKG_VERSION}"

export VCPKG_ROOT=/vcpkg

if [[ ! -d "${VCPKG_ROOT}/.git" ]]; then
  git clone \
    --branch "${VCPKG_VERSION}" \
    https://github.com/microsoft/vcpkg.git \
    "${VCPKG_ROOT}"
fi

cd "${VCPKG_ROOT}"

git checkout "${VCPKG_VERSION}"

./bootstrap-vcpkg.sh -disableMetrics

cd "${WORKSPACE_DIR}"

# ============================================================
# COMMON CMAKE ARGUMENTS
# ============================================================

mapfile -t cmake_args < <(cmake::common_args)

# ============================================================
# BENCHMARK CONFIGURATION
# ============================================================

BUILD_DIR="${WORKSPACE_DIR}/cmake-out"

export ODBC_TESTS_DSN="SampleDSNGoogleDriver"
export ODBC_TRANSACTIONS_TESTS_DSN="ODBCTransactionsTestsDSN"

export CPP_BIGQUERY_ODBC_TEST_TABLE_PREFIX="${TRIGGER_NAME//[-:;.,?]/_}_${BRANCH_NAME//[-:;.,?]/_}"

BENCHMARK_ITERATIONS="${BENCHMARK_ITERATIONS:-1}"

echo "============================================================"
echo "Benchmark configuration"
echo "============================================================"
echo "Workspace       : ${WORKSPACE_DIR}"
echo "Build directory : ${BUILD_DIR}"
echo "Iterations      : ${BENCHMARK_ITERATIONS}"
echo "Google DSN      : ${ODBC_TESTS_DSN}"
echo "============================================================"

# ============================================================
# UNIXODBC
# ============================================================

if command -v odbcinst &>/dev/null; then
  export UNIXODBC_INSTALLED=true
  echo "unixODBC is installed."
else
  export UNIXODBC_INSTALLED=false
  export ODBCINSTINI=/opt/odbc-driver/odbcinst.ini
  echo "unixODBC is not installed."
  echo "Using ODBCINSTINI=${ODBCINSTINI}"
fi

# ============================================================
# CONFIGURE CMAKE
#
# IMPORTANT:
# This is a separate benchmark build.
#
# BUILD_PERFORMANCE_TEST_ONLY=ON means we are configuring the
# performance-test build rather than configuring/running the
# complete integration-test suite.
# ============================================================

echo "============================================================"
echo "Configuring performance_test"
echo "============================================================"

io::run cmake -S "${WORKSPACE_DIR}" -B "${BUILD_DIR}" \
  "${cmake_args[@]}" \
  -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" \
  -DCMAKE_CXX_STANDARD=17 \
  -DODBC_INTEGRATION_TESTING=ON \
  -DBQ_DRIVER_INTEGRATION_TESTS=ON \
  -DODBC_DEMO_TESTING=ON \
  -DODBC_EXAMPLES=ON \
  -DODBC_UNIT_TESTING=OFF \
  -DCLIENT_LIBRARY_INTEGRATION_TESTING=OFF

# ============================================================
# BUILD ONLY performance_test
# ============================================================

echo "============================================================"
echo "Building ONLY performance_test"
echo "============================================================"

io::run cmake --build "${BUILD_DIR}" \
  --target google_cloud_odbc_bq_driver \
  --config Release \
  --parallel "$(nproc)"

io::run cmake --build "${BUILD_DIR}" \
  --target performance_test \
  --config Release \
  --parallel "$(nproc)"

# ============================================================
# LOCATE PERFORMANCE TEST
# ============================================================

PERFORMANCE_TEST="${BUILD_DIR}/google/cloud/odbc/integration_tests/performance_test"

if [[ ! -f "${PERFORMANCE_TEST}" ]]; then
  echo "ERROR: performance_test was not found:"
  echo "  ${PERFORMANCE_TEST}"
  echo
  echo "Searching build directory:"
  find "${BUILD_DIR}" -type f -name "performance_test*" -print
  exit 1
fi

echo "============================================================"
echo "performance_test found"
echo "============================================================"
echo "${PERFORMANCE_TEST}"

# ============================================================
# GOOGLE DRIVER
#
# The Google driver is generated by the performance_test build.
# We do NOT download an existing Google .so.
# ============================================================

GOOGLE_DRIVER="${BUILD_DIR}/google/cloud/odbc/libgoogle_cloud_odbc_bq_driver.so"

if [[ ! -f "${GOOGLE_DRIVER}" ]]; then
  echo "ERROR: Google driver was not generated by the performance build:"
  echo "  ${GOOGLE_DRIVER}"
  echo
  echo "Generated shared libraries:"
  find "${BUILD_DIR}" -type f -name "*.so" -print
  exit 1
fi

echo "============================================================"
echo "Google driver generated"
echo "============================================================"
echo "${GOOGLE_DRIVER}"

# ============================================================
# CERTIFICATE
# ============================================================

GOOGLE_DRIVER_DIR="${BUILD_DIR}/google/cloud/odbc"

if [[ ! -f /opt/odbc-driver/roots.pem ]]; then
  echo "ERROR: /opt/odbc-driver/roots.pem was not found."
  exit 1
fi

io::run cp \
  /opt/odbc-driver/roots.pem \
  "${GOOGLE_DRIVER_DIR}/roots.pem"

# ============================================================
# ODBC CONFIGURATION
#
# The existing /opt/odbc-driver/odbc.ini must contain the DSNs
# used below.
#
# Google:
#   SampleDSNGoogleDriver
#
# Simba:
#   SampleDSN
#
# If SampleDSN does not exist, Simba cannot be benchmarked.
# ============================================================

ODBC_INI="/opt/odbc-driver/odbc.ini"

if [[ ! -f "${ODBC_INI}" ]]; then
  echo "ERROR: ODBC configuration was not found:"
  echo "  ${ODBC_INI}"
  exit 1
fi

export ODBCINI="${ODBC_INI}"

echo "============================================================"
echo "ODBC configuration"
echo "============================================================"
echo "ODBCINI=${ODBCINI}"

echo
echo "Available DSNs:"
grep -E '^\[[^]]+\]$' "${ODBCINI}" || true

# ============================================================
# VERIFY GOOGLE DSN
# ============================================================

if ! grep -q '^\[SampleDSNGoogleDriver\]$' "${ODBC_INI}"; then
  echo
  echo "ERROR: Google DSN [SampleDSNGoogleDriver] was not found in:"
  echo "  ${ODBC_INI}"
  exit 1
fi

# ============================================================
# VERIFY SIMBA DSN
# ============================================================

if ! grep -q '^\[SampleDSN\]$' "${ODBC_INI}"; then
  echo
  echo "ERROR: Simba DSN [SampleDSN] was not found in:"
  echo "  ${ODBC_INI}"
  echo
  echo "The performance executable can be reused for Simba, but"
  echo "the Simba DSN must be available in ODBCINI."
  exit 1
fi

# ============================================================
# RESULT FILES
# ============================================================

RESULT_DIR="${WORKSPACE_DIR}/benchmark_results"

mkdir -p "${RESULT_DIR}"

GOOGLE_RESULTS="${RESULT_DIR}/current_bq.txt"
SIMBA_RESULTS="${RESULT_DIR}/current_core.txt"

MAIN_GOOGLE_RESULTS="${RESULT_DIR}/main_bq.txt"

SUMMARY_FILE="${WORKSPACE_DIR}/benchmark_summary_table.txt"

rm -f \
  "${GOOGLE_RESULTS}" \
  "${SIMBA_RESULTS}" \
  "${MAIN_GOOGLE_RESULTS}" \
  "${SUMMARY_FILE}"

# ============================================================
# RUN GOOGLE DRIVER
#
# IMPORTANT:
#
# The executable is invoked directly.
#
# This executes the COMPLETE performance_test suite.
#
# There is NO ctest here.
#
# There is NO individual test filter.
#
# With BENCHMARK_ITERATIONS=1:
#
#     performance_test
#
# runs exactly once, and every test registered in that executable
# is executed once.
# ============================================================

export ODBC_TESTS_DSN="SampleDSNGoogleDriver"

echo "============================================================"
echo "Running Google Driver performance_test"
echo "============================================================"
echo "DSN       : ${ODBC_TESTS_DSN}"
echo "Iterations: ${BENCHMARK_ITERATIONS}"
echo "Executable: ${PERFORMANCE_TEST}"
echo "============================================================"

GOOGLE_EXIT_CODE=0

for ((i = 1; i <= BENCHMARK_ITERATIONS; i++)); do

  echo "=== Google Driver iteration ${i}/${BENCHMARK_ITERATIONS} ===" \
    >>"${GOOGLE_RESULTS}"

  set +e

  "${PERFORMANCE_TEST}" >>"${GOOGLE_RESULTS}" 2>&1

  RUN_EXIT_CODE=$?

  set -e

  if [[ ${RUN_EXIT_CODE} -ne 0 ]]; then
    echo "WARNING: Google Driver iteration ${i} failed with exit code ${RUN_EXIT_CODE}"
    GOOGLE_EXIT_CODE=${RUN_EXIT_CODE}
  fi

done

if [[ ${GOOGLE_EXIT_CODE} -ne 0 ]]; then
  echo
  echo "ERROR: Google Driver performance_test failed."
  echo
  echo "Last Google benchmark output:"
  tail -100 "${GOOGLE_RESULTS}" || true
  exit "${GOOGLE_EXIT_CODE}"
fi

echo "Google Driver performance suite completed successfully."

# ============================================================
# RUN SIMBA DRIVER
#
# SAME performance_test executable.
#
# Only the DSN changes.
#
# Therefore the exact same performance test cases are executed
# against Simba.
# ============================================================

export ODBC_TESTS_DSN="SampleDSN"

echo "============================================================"
echo "Running Simba Driver performance_test"
echo "============================================================"
echo "DSN       : ${ODBC_TESTS_DSN}"
echo "Iterations: ${BENCHMARK_ITERATIONS}"
echo "Executable: ${PERFORMANCE_TEST}"
echo "============================================================"

SIMBA_EXIT_CODE=0

for ((i = 1; i <= BENCHMARK_ITERATIONS; i++)); do

  echo "=== Simba Driver iteration ${i}/${BENCHMARK_ITERATIONS} ===" \
    >>"${SIMBA_RESULTS}"

  set +e

  "${PERFORMANCE_TEST}" >>"${SIMBA_RESULTS}" 2>&1

  RUN_EXIT_CODE=$?

  set -e

  if [[ ${RUN_EXIT_CODE} -ne 0 ]]; then
    echo "WARNING: Simba Driver iteration ${i} failed with exit code ${RUN_EXIT_CODE}"
    SIMBA_EXIT_CODE=${RUN_EXIT_CODE}
  fi

done

if [[ ${SIMBA_EXIT_CODE} -ne 0 ]]; then
  echo
  echo "ERROR: Simba Driver performance_test failed."
  echo
  echo "Last Simba benchmark output:"
  tail -100 "${SIMBA_RESULTS}" || true
  exit "${SIMBA_EXIT_CODE}"
fi

echo "Simba Driver performance suite completed successfully."

# ============================================================
# DOWNLOAD MAIN GOOGLE DRIVER RESULTS
# ============================================================

CURRENT_BRANCH="${BRANCH_NAME:-main}"

SANITIZED_BRANCH="$(echo "${CURRENT_BRANCH}" |
  sed 's/[^a-zA-Z0-9._-]/_/g')"

echo "============================================================"
echo "Downloading Google Driver main baseline"
echo "============================================================"

set +e

gcloud storage cp \
  "gs://bq-dev-tools-testing-drivers/odbc-perf/main/results/performance_benchmark_results_BqDriver.txt" \
  "${MAIN_GOOGLE_RESULTS}"

MAIN_DOWNLOAD_EXIT=$?

set -e

if [[ ${MAIN_DOWNLOAD_EXIT} -ne 0 ]]; then
  echo "WARNING: Main Google benchmark result is not available."
  rm -f "${MAIN_GOOGLE_RESULTS}"
else
  echo "Main Google benchmark downloaded successfully."
fi

# ============================================================
# PARSE AND COMPARE
#
# Comparison:
#
#   Simba Driver
#          |
#          | Google Current vs Simba
#          v
#   Google Driver Current
#          |
#          | Google Main vs Google Current
#          v
#   Google Driver Main
#
# Every test appearing in any of the three result files is
# included in the table.
# ============================================================

echo "============================================================"
echo "Generating performance comparison"
echo "============================================================"

python3 <<'PYTHON'
import os
import re


SIMBA_FILE = "benchmark_results/current_core.txt"
GOOGLE_FILE = "benchmark_results/current_bq.txt"
MAIN_FILE = "benchmark_results/main_bq.txt"

OUTPUT_FILE = "benchmark_summary_table.txt"


def clean_test_name(name):
    """
    GTest names can look like:

      TestSuite.TestCase
      Instantiation/TestSuite.TestCase
      TestSuite.TestCase/parameter

    Keep the TestCase portion so the same test can be compared
    between the two drivers.
    """

    if "." in name:
        name = name.split(".", 1)[1]

    # Remove old HTAPI parameter suffixes.
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

    return None


def parse_gtest_output(filename):
    """
    Parse:

      [       OK ] TestSuite.TestCase (123 ms)

    Multiple iterations are supported.

    The median is used if BENCHMARK_ITERATIONS > 1.
    """

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

    results = {}

    for test_name, values in samples.items():

        values.sort()

        n = len(values)

        if n % 2 == 1:
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

    if simba_ms is None:
        simba_value = "N/A"
    else:
        simba_value = f"{simba_ms:.0f}ms"

    if google_ms is None:
        google_value = "N/A"
    else:
        google_value = (
            f"{google_ms:.0f}ms "
            f"({google_vs_simba})"
        )

    if main_ms is None:
        main_value = "N/A"
    else:
        main_value = (
            f"{main_ms:.0f}ms "
            f"({main_vs_google})"
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
        width = max(
            width,
            len(row[index])
        )

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
    "Negative values indicate that the Google Driver is faster; "
    "positive values indicate that it is slower."
)


output = (
    description
    + "\n\n"
    + "\n".join(table)
    + "\n"
)


with open(OUTPUT_FILE, "w") as file:
    file.write(output)


print(output)

print(
    f"Compared {len(all_tests)} performance test cases."
)

PYTHON

# ============================================================
# UPLOAD RESULTS
# ============================================================

RESULTS_BUCKET="gs://bq-dev-tools-testing-drivers/odbc-perf/${SANITIZED_BRANCH}/results"

echo "============================================================"
echo "Uploading benchmark results"
echo "============================================================"
echo "Destination:"
echo "  ${RESULTS_BUCKET}"
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

# ============================================================
# FINAL SUMMARY
# ============================================================

echo
echo "============================================================"
echo "BENCHMARK COMPLETED SUCCESSFULLY"
echo "============================================================"

echo
echo "Performance executable:"
echo "  ${PERFORMANCE_TEST}"

echo
echo "Google Driver:"
echo "  ${GOOGLE_DRIVER}"

echo
echo "Google results:"
echo "  ${GOOGLE_RESULTS}"

echo
echo "Simba results:"
echo "  ${SIMBA_RESULTS}"

echo
echo "Comparison:"
echo "  ${SUMMARY_FILE}"

echo
echo "GCS:"
echo "  ${RESULTS_BUCKET}/"

echo
echo "All tests registered in performance_test were executed."
echo "Benchmark iterations: ${BENCHMARK_ITERATIONS}"

echo "============================================================"
