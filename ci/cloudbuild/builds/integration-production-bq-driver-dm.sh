# linux-bq-driver-benchmark.sh

```bash
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

WORKSPACE_DIR=$(pwd)
BUILD_DIR="${WORKSPACE_DIR}/cmake-out"

# Number of times to run the COMPLETE performance_test suite.
# Default 1 means every performance test runs exactly once.
BENCHMARK_ITERATIONS="${BENCHMARK_ITERATIONS:-1}"

# Branch used for storing current benchmark results.
CURRENT_BRANCH="${BRANCH_NAME:-main}"

SANITIZED_BRANCH=$(echo "${CURRENT_BRANCH}" | \
  sed 's/[^a-zA-Z0-9._-]/_/g')

# ------------------------------------------------------------
# Benchmark result locations
# ------------------------------------------------------------

RESULTS_DIR="${WORKSPACE_DIR}/benchmark_results"
mkdir -p "${RESULTS_DIR}"

SIMBA_RESULTS="${RESULTS_DIR}/performance_benchmark_results_Core.txt"
GOOGLE_RESULTS="${RESULTS_DIR}/performance_benchmark_results_BqDriver.txt"
MAIN_GOOGLE_RESULTS="${RESULTS_DIR}/main_bq.txt"

SUMMARY_FILE="${RESULTS_DIR}/benchmark_summary_table.txt"

# ------------------------------------------------------------
# Header
# ------------------------------------------------------------

echo "============================================================"
echo "Linux ODBC Performance Benchmark Comparison"
echo "============================================================"
echo "Current branch       : ${CURRENT_BRANCH}"
echo "Benchmark iterations : ${BENCHMARK_ITERATIONS}"
echo "Build directory      : ${BUILD_DIR}"
echo
echo "Comparison:"
echo "  1. Simba Driver  - Current Branch"
echo "  2. Google Driver - Current Branch"
echo "  3. Google Driver - Main Branch"
echo "============================================================"

# ------------------------------------------------------------
# Vcpkg
# ------------------------------------------------------------

VCPKG_VERSION=$(cat /tmp/vcpkg-version.txt)
export VCPKG_VERSION

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

if [[ ! -f "${VCPKG_ROOT}/vcpkg" ]]; then
  ./bootstrap-vcpkg.sh -disableMetrics
fi

cd "${WORKSPACE_DIR}"

# ------------------------------------------------------------
# CMake arguments
# ------------------------------------------------------------

mapfile -t cmake_args < <(cmake::common_args)

# ------------------------------------------------------------
# Configure the normal Google Driver build
#
# DO NOT use BUILD_PERFORMANCE_TEST_ONLY here.
#
# The benchmark executable must be able to test the current
# branch Google driver, so the normal driver build is required.
# ------------------------------------------------------------

echo
echo "============================================================"
echo "Configuring current branch build"
echo "============================================================"

rm -rf "${BUILD_DIR}"

io::run cmake \
  -S "${WORKSPACE_DIR}" \
  -B "${BUILD_DIR}" \
  "${cmake_args[@]}" \
  -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_STANDARD=17 \
  -DODBC_INTEGRATION_TESTING=ON \
  -DBQ_DRIVER_INTEGRATION_TESTS=ON \
  -DODBC_DEMO_TESTING=ON \
  -DODBC_EXAMPLES=ON \
  -DODBC_UNIT_TESTING=OFF \
  -DCLIENT_LIBRARY_INTEGRATION_TESTING=OFF

# ------------------------------------------------------------
# Build ONLY performance_test
#
# This does not run ctest and does not explicitly build every
# project target. CMake builds performance_test and its required
# dependencies.
# ------------------------------------------------------------

echo
echo "============================================================"
echo "Building performance_test"
echo "============================================================"

io::run cmake \
  --build "${BUILD_DIR}" \
  --target performance_test \
  --parallel "$(nproc)"

# ------------------------------------------------------------
# Locate performance_test
# ------------------------------------------------------------

PERFORMANCE_TEST="${BUILD_DIR}/google/cloud/odbc/integration_tests/performance_test"

if [[ ! -f "${PERFORMANCE_TEST}" ]]; then
  echo
  echo "ERROR: performance_test was not found:"
  echo "  ${PERFORMANCE_TEST}"
  echo
  echo "Matching files:"
  find "${BUILD_DIR}" -type f -name "performance_test*" -print || true
  exit 1
fi

echo
echo "Performance test executable:"
echo "  ${PERFORMANCE_TEST}"

# ------------------------------------------------------------
# Verify current branch Google Driver
# ------------------------------------------------------------

GOOGLE_DRIVER="${BUILD_DIR}/google/cloud/odbc/libgoogle_cloud_odbc_bq_driver.so"

if [[ ! -f "${GOOGLE_DRIVER}" ]]; then
  echo
  echo "ERROR: Current branch Google Driver was not generated:"
  echo "  ${GOOGLE_DRIVER}"
  echo
  echo "Matching driver files:"
  find "${BUILD_DIR}" \
    -type f \
    \( -name "*google_cloud_odbc_bq_driver*.so" \
       -o -name "*bq_driver*.so" \) \
    -print || true
  exit 1
fi

echo
echo "Current branch Google Driver:"
echo "  ${GOOGLE_DRIVER}"

# ------------------------------------------------------------
# Copy roots.pem for Google Driver
# ------------------------------------------------------------

GOOGLE_DRIVER_DIR="${BUILD_DIR}/google/cloud/odbc"

if [[ -f "/opt/odbc-driver/roots.pem" ]]; then
  io::run cp \
    "/opt/odbc-driver/roots.pem" \
    "${GOOGLE_DRIVER_DIR}/roots.pem"
else
  echo
  echo "ERROR: Required certificate file was not found:"
  echo "  /opt/odbc-driver/roots.pem"
  exit 1
fi

# ------------------------------------------------------------
# ODBC environment
# ------------------------------------------------------------

export ODBCINI="/opt/odbc-driver/odbc.ini"

if [[ ! -f "${ODBCINI}" ]]; then
  echo
  echo "ERROR: ODBC configuration was not found:"
  echo "  ${ODBCINI}"
  exit 1
fi

echo
echo "Using ODBCINI=${ODBCINI}"

# ------------------------------------------------------------
# unixODBC / iODBC setup
# ------------------------------------------------------------

if command -v odbcinst >/dev/null 2>&1; then
  export UNIXODBC_INSTALLED=true
  echo "unixODBC is installed."
else
  export UNIXODBC_INSTALLED=false
  export ODBCINSTINI="/opt/odbc-driver/odbcinst.ini"
  echo "unixODBC is not installed."
fi

# ------------------------------------------------------------
# Unique test table prefix
# ------------------------------------------------------------

export CPP_BIGQUERY_ODBC_TEST_TABLE_PREFIX=\
"${TRIGGER_NAME:-benchmark}_${SANITIZED_BRANCH}"

# ------------------------------------------------------------
# Helper: verify a DSN exists
# ------------------------------------------------------------

verify_dsn() {
  local dsn="$1"

  if ! grep -q "^\[${dsn}\]$" "${ODBCINI}"; then
    echo
    echo "ERROR: DSN not found in ${ODBCINI}:"
    echo "  [${dsn}]"
    echo
    echo "Available DSNs:"
    grep '^\[.*\]$' "${ODBCINI}" || true
    exit 1
  fi
}

# ------------------------------------------------------------
# Helper: run complete performance_test suite
#
# One invocation of performance_test runs every test registered
# in that executable.
#
# BENCHMARK_ITERATIONS=1:
#   every performance_test test executes once.
# ------------------------------------------------------------

run_performance_suite() {
  local driver_name="$1"
  local dsn="$2"
  local output_file="$3"

  export ODBC_TESTS_DSN="${dsn}"

  verify_dsn "${ODBC_TESTS_DSN}"

  echo
  echo "============================================================"
  echo "Running ${driver_name} performance suite"
  echo "============================================================"
  echo "DSN       : ${ODBC_TESTS_DSN}"
  echo "Iterations: ${BENCHMARK_ITERATIONS}"
  echo "Executable: ${PERFORMANCE_TEST}"
  echo "Output    : ${output_file}"
  echo "============================================================"

  : > "${output_file}"

  local test_exit_code=0
  local run_exit=0

  for ((i = 1; i <= BENCHMARK_ITERATIONS; i++)); do

    echo
    echo "${driver_name}: iteration ${i}/${BENCHMARK_ITERATIONS}"

    echo "=== benchmark iteration ${i}/${BENCHMARK_ITERATIONS} ===" \
      >> "${output_file}"

    set +e

    "${PERFORMANCE_TEST}" \
      >> "${output_file}" \
      2>&1

    run_exit=$?

    set -e

    if [[ ${run_exit} -ne 0 ]]; then
      echo
      echo "WARNING: ${driver_name} iteration ${i} failed with exit code ${run_exit}"
      test_exit_code=${run_exit}
    fi
  done

  if [[ ${test_exit_code} -ne 0 ]]; then
    echo
    echo "ERROR: ${driver_name} performance_test failed."
    echo
    echo "Last benchmark output:"
    tail -n 50 "${output_file}" || true
    return "${test_exit_code}"
  fi
}

# ------------------------------------------------------------
# 1. Run Simba Driver - Current Branch
#
# Simba is loaded through the existing SampleDSN configuration.
# ------------------------------------------------------------

run_performance_suite \
  "Simba Driver (Current)" \
  "SampleDSN" \
  "${SIMBA_RESULTS}"

# ------------------------------------------------------------
# 2. Run Google Driver - Current Branch
#
# IMPORTANT:
#
# SampleDSNGoogleDriver must point to:
#
#   ${GOOGLE_DRIVER}
#
# in /opt/odbc-driver/odbc.ini.
# ------------------------------------------------------------

run_performance_suite \
  "Google Driver (Current)" \
  "SampleDSNGoogleDriver" \
  "${GOOGLE_RESULTS}"

# ------------------------------------------------------------
# 3. Download Google Driver Main Branch results
#
# Main benchmark results are produced by the main branch build
# and used as the baseline here.
# ------------------------------------------------------------

echo
echo "============================================================"
echo "Downloading Google Driver main branch benchmark results"
echo "============================================================"

set +e

gcloud storage cp \
  "gs://bq-dev-tools-testing-drivers/odbc-perf/main/results/performance_benchmark_results_BqDriver.txt" \
  "${MAIN_GOOGLE_RESULTS}"

MAIN_DOWNLOAD_EXIT=$?

set -e

if [[ ${MAIN_DOWNLOAD_EXIT} -ne 0 ]]; then
  echo
  echo "WARNING: Main branch benchmark result was not available."
  rm -f "${MAIN_GOOGLE_RESULTS}"
fi

# ------------------------------------------------------------
# Generate comparison table
# ------------------------------------------------------------

echo
echo "============================================================"
echo "Generating benchmark comparison"
echo "============================================================"

SIMBA_FILE="${SIMBA_RESULTS}" \
GOOGLE_FILE="${GOOGLE_RESULTS}" \
MAIN_FILE="${MAIN_GOOGLE_RESULTS}" \
OUTPUT_FILE="${SUMMARY_FILE}" \
python3 <<'PYTHON'
import os
import re


SIMBA_FILE = os.environ["SIMBA_FILE"]
GOOGLE_FILE = os.environ["GOOGLE_FILE"]
MAIN_FILE = os.environ["MAIN_FILE"]
OUTPUT_FILE = os.environ["OUTPUT_FILE"]


def clean_test_name(name):
    # GTest name:
    # [Instantiation/]TestSuite.TestCase[/Param]
    #
    # Remove the suite/instantiation prefix so corresponding
    # benchmark cases can be compared.

    if "." in name:
        name = name.split(".", 1)[1]

    # Preserve compatibility with older output.
    name = re.sub(r"/(?:With|Without)HTAPI$", "", name)

    return name


def parse_time_to_ms(value):
    if not value:
        return None

    value = value.strip()

    match = re.match(r"^([\d.]+)\s*(\w+)$", value)

    if not match:
        return None

    number = float(match.group(1))
    unit = match.group(2).lower()

    if unit == "s":
        return number * 1000.0

    if unit == "ms":
        return number

    if unit == "us":
        return number / 1000.0

    if unit == "ns":
        return number / 1000000.0

    return None


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
            duration_ms = parse_time_to_ms(match.group(2))

            if duration_ms is None:
                continue

            samples.setdefault(test_name, []).append(duration_ms)

    results = {}

    for test_name, values in samples.items():
        values.sort()

        count = len(values)

        if count % 2 == 1:
            median = values[count // 2]
        else:
            median = (
                values[count // 2 - 1] +
                values[count // 2]
            ) / 2.0

        results[test_name] = median

    return results


def percentage(value, reference):
    if value is None or reference is None or reference == 0:
        return "N/A"

    change = ((value - reference) / reference) * 100.0

    return f"{change:+.0f}%"


simba = parse_gtest_output(SIMBA_FILE)
google = parse_gtest_output(GOOGLE_FILE)
main_google = parse_gtest_output(MAIN_FILE)

all_tests = sorted(
    set(simba) |
    set(google) |
    set(main_google)
)

rows = []

for test_name in all_tests:
    simba_ms = simba.get(test_name)
    google_ms = google.get(test_name)
    main_ms = main_google.get(test_name)

    simba_value = (
        f"{simba_ms:.0f}ms"
        if simba_ms is not None
        else "N/A"
    )

    google_value = (
        f"{google_ms:.0f}ms"
        if google_ms is not None
        else "N/A"
    )

    main_value = (
        f"{main_ms:.0f}ms"
        if main_ms is not None
        else "N/A"
    )

    google_pct = percentage(google_ms, simba_ms)
    main_pct = percentage(main_ms, google_ms)

    if google_ms is not None:
        google_value += f" ({google_pct})"

    if main_ms is not None:
        main_value += f" ({main_pct})"

    rows.append(
        (
            test_name,
            simba_value,
            google_value,
            main_value,
        )
    )


headers = (
    "Test Case",
    "Simba Driver (Current)",
    "Google Driver (Current)",
    "Google Driver (Main)",
)

widths = []

for index, header in enumerate(headers):
    width = len(header)

    for row in rows:
        width = max(width, len(row[index]))

    widths.append(width)


table = []

description = (
    "Only tests executed by the performance_test executable are included. "
    "Percentages in Google Driver (Current) are relative to Simba Driver "
    "(Current). Percentages in Google Driver (Main) are relative to Google "
    "Driver (Current). Negative values are faster and positive values are slower."
)

table.append(description)
table.append("")

table.append(
    "| " +
    " | ".join(
        header.ljust(widths[index])
        for index, header in enumerate(headers)
    ) +
    " |"
)

table.append(
    "|-" +
    "-|-".join(
        "-" * widths[index]
        for index in range(len(headers))
    ) +
    "-|"
)

for row in rows:
    table.append(
        "| " +
        " | ".join(
            row[index].ljust(widths[index])
            for index in range(len(headers))
        ) +
        " |"
    )


output = "\n".join(table) + "\n"

with open(OUTPUT_FILE, "w") as file:
    file.write(output)

print(output)
print(f"Compared {len(all_tests)} performance test cases.")

PYTHON

# ------------------------------------------------------------
# Upload results
# ------------------------------------------------------------

RESULTS_BUCKET="gs://bq-dev-tools-testing-drivers/odbc-perf/${SANITIZED_BRANCH}/results"

echo
echo "============================================================"
echo "Uploading benchmark results"
echo "============================================================"

io::run gcloud storage cp \
  "${SIMBA_RESULTS}" \
  "${RESULTS_BUCKET}/performance_benchmark_results_Core.txt"

io::run gcloud storage cp \
  "${GOOGLE_RESULTS}" \
  "${RESULTS_BUCKET}/performance_benchmark_results_BqDriver.txt"

io::run gcloud storage cp \
  "${SUMMARY_FILE}" \
  "${RESULTS_BUCKET}/benchmark_summary_table.txt"

echo
echo "============================================================"
echo "Linux benchmark comparison completed successfully"
echo "============================================================"
echo
echo "Simba current:"
echo "  ${SIMBA_RESULTS}"
echo
echo "Google current:"
echo "  ${GOOGLE_RESULTS}"
echo
echo "Google main:"
echo "  ${MAIN_GOOGLE_RESULTS}"
echo
echo "Comparison:"
echo "  ${SUMMARY_FILE}"
echo
echo "Uploaded to:"
echo "  ${RESULTS_BUCKET}/"
echo "============================================================"
```
