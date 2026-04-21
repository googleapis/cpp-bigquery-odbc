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

# Save current workspace path
WORKSPACE_DIR=$(pwd)

# Read and export VCPKG version from file
VCPKG_VERSION=$(cat /tmp/vcpkg-version.txt)
export VCPKG_VERSION
echo "Using VCPKG_VERSION=$VCPKG_VERSION"

# Vcpkg install and configure
export VCPKG_ROOT=/vcpkg
git clone --branch "$VCPKG_VERSION" https://github.com/microsoft/vcpkg.git "$VCPKG_ROOT"
cd "$VCPKG_ROOT"
git checkout "$VCPKG_VERSION"

./bootstrap-vcpkg.sh -disableMetrics

cd "$WORKSPACE_DIR"
# This runs all the unit tests
mapfile -t args < <(bazel::common_args)
mapfile -t unit_tests_args < <(unit_tests::bazel_args)
mapfile -t secrets_bazel < <(secrets::bazel_args)

io::run bazel test "${args[@]}" "${secrets_bazel[@]}" "${unit_tests_args[@]}" --test_tag_filters=unit-tests ...

if [[ -n "${TAG_NAME:-}" ]]; then
  VERSION="${TAG_NAME#v}"
  echo "Version from Git tag: $VERSION"
else
  VERSION="1.0.0"
  echo "Warning: TAG_NAME and SHORT_SHA not found. Using default version: ${VERSION}"
fi

# Run the integration tests
mapfile -t cmake_args < <(cmake::common_args)

BUILD_DIR="/opt/odbc-driver"
# This is the name of DSN set in odbc.ini
export ODBC_TESTS_DSN="SampleDSNGoogleDriver"
export CPP_BIGQUERY_ODBC_TEST_TABLE_PREFIX=${TRIGGER_NAME//[-:;.,?]/_}_${BRANCH_NAME//[-:;.,?]/_}
export ODBCINSTINI=/opt/odbc-driver/odbcinst.ini

io::run cmake -B "$BUILD_DIR" \
  "${cmake_args[@]}" \
  -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" \
  -DCMAKE_CXX_STANDARD=17 \
  -DODBC_INTEGRATION_TESTING=ON \
  -DBQ_DRIVER_INTEGRATION_TESTS=ON \
  -DODBC_DEMO_TESTING=OFF \
  -DODBC_EXAMPLES=ON \
  -DODBC_UNIT_TESTING=OFF \
  -DBUILD_SHARED_LIBS=ON \
  -DCLIENT_LIBRARY_INTEGRATION_TESTING=OFF \
  -DCMAKE_BUILD_TYPE=Release \
  -DPROJECT_VERSION="${VERSION}"

io::run cmake --build cmake-out
# Copy the roots.pem file to the .so directory to run test cases.
cp /opt/odbc-driver/roots.pem "cmake-out/google/cloud/odbc/roots.pem"
mapfile -t ctest_args < <(ctest::common_args)


DRIVER="cmake-out/google/cloud/odbc/libgoogle_cloud_odbc_bq_driver.so"

echo "=== Checking Arrow linking for: $DRIVER ==="
echo ""

echo "1. ldd check (look for libarrow.so):"
ldd $DRIVER | grep -q arrow && echo "  DYNAMIC: Found Arrow library dependency" || echo "  STATIC: No Arrow library dependency"

echo ""
echo "2. objdump dynamic symbols check:"
objdump -T $DRIVER 2>/dev/null | grep -q arrow && echo "  DYNAMIC: Arrow symbols in dynamic table" || echo "  STATIC: No Arrow dynamic symbols"

echo ""
echo "3. readelf NEEDED libraries check:"
readelf -d $DRIVER 2>/dev/null | grep NEEDED | grep -q arrow && echo "  DYNAMIC: Arrow in NEEDED libraries" || echo "  STATIC: Arrow not in NEEDED libraries"

echo ""
echo "4. File size:"
ls -lh $DRIVER | awk '{print "  " $5 " - " $9}'

echo ""
echo "5. Check for undefined Arrow symbols:"
UNDEF_COUNT=$(nm -D $DRIVER 2>/dev/null | grep arrow | grep " U " | wc -l)
if [ $UNDEF_COUNT -gt 0 ]; then
    echo "  DYNAMIC: Found $UNDEF_COUNT undefined Arrow symbols"
else
    echo "  STATIC: No undefined Arrow symbols"
fi








io::run env -C cmake-out ctest "${ctest_args[@]}"

io::log_h1 "Packaging and Uploading Driver"

RELEASE_DIR="release_package"
mkdir -p "${RELEASE_DIR}/lib"

# Copy driver files
io::run cp -v "/workspace/cmake-out/google/cloud/odbc/libgoogle_cloud_odbc_bq_driver.so" "${RELEASE_DIR}/lib/libgoogle_cloud_odbc_bq_driver.so"

# Copy ODBC config file templates
io::run cp -v "/opt/odbc-driver/odbc_template.ini" "${RELEASE_DIR}/odbc.ini"
io::run cp -v "/opt/odbc-driver/odbcinst_template.ini" "${RELEASE_DIR}/odbcinst.ini"
io::run cp -v "/opt/odbc-driver/googlebigqueryodbc.ini" "${RELEASE_DIR}/googlebigqueryodbc.ini"

# Copy root certificates
io::run cp -v "/opt/odbc-driver/roots.pem" "${RELEASE_DIR}/roots.pem"

# Create ZIP file
ZIP_NAME="odbc-driver.${VERSION}.zip"
cd "${RELEASE_DIR}"
io::run zip -r "../${ZIP_NAME}" .
cd ..
io::log "ZIP package created: ${ZIP_NAME}"

# Upload to GCS
export GCS_BUCKET=bq_devtools_release_private
io::log "Uploading ${ZIP_NAME} to gs://${GCS_BUCKET}/drivers/odbc/linux/"
io::run gsutil -m cp "${ZIP_NAME}" "gs://${GCS_BUCKET}/drivers/odbc/linux/"
