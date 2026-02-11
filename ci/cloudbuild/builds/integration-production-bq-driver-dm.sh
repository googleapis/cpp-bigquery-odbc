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

ARCH="$(uname -m)"
JOBS="$(nproc)"

echo "========================================"
echo "🧠 System architecture diagnostics"
echo "uname -m        : ${ARCH}"
echo "========================================"
# -----------------------------------------
# ARM64 hardening (vcpkg + grpc)
# -----------------------------------------
export VCPKG_BUILD_TYPE=release
export VCPKG_MAX_CONCURRENCY="${JOBS}"
export CMAKE_BUILD_PARALLEL_LEVEL="${JOBS}"
export VCPKG_DISABLE_METRICS=1
export VCPKG_FEATURE_FLAGS=manifests,versions

# Force Ninja everywhere
export CMAKE_MAKE_PROGRAM=/usr/bin/ninja
export PATH=/usr/bin:$PATH

# Required by grpc on arm64
if command -v apt-get &>/dev/null; then
  apt-get update
  apt-get install -y ninja-build libatomic1
fi

if [[ "$ARCH" == "aarch64" || "$ARCH" == "arm64" ]]; then
  echo "🔥 ARM64 detected — Bazel disabled"
else
  mapfile -t args < <(bazel::common_args)
  mapfile -t unit_tests_args < <(unit_tests::bazel_args)
  mapfile -t secrets_bazel < <(secrets::bazel_args)

  io::run bazel test \
    "${args[@]}" \
    "${secrets_bazel[@]}" \
    "${unit_tests_args[@]}" \
    --test_tag_filters=unit-tests ...
fi

# -----------------------------------------
# CMake configure
# -----------------------------------------
mapfile -t cmake_args < <(cmake::common_args)

export ODBC_TESTS_DSN="SampleDSN"
export CPP_BIGQUERY_ODBC_TEST_TABLE_PREFIX=${TRIGGER_NAME//[-:;.,?]/_}_${BRANCH_NAME//[-:;.,?]/_}

io::run cmake "${cmake_args[@]}" \
  -GNinja \
  -DCMAKE_MAKE_PROGRAM=/usr/bin/ninja \
  -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=arm64-linux \
  -DVCPKG_HOST_TRIPLET=arm64-linux \
  -DVCPKG_BUILD_TYPE=release \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_STANDARD=17 \
  -DODBC_INTEGRATION_TESTING=ON \
  -DBQ_DRIVER_INTEGRATION_TESTS=ON \
  -DODBC_DEMO_TESTING=OFF \
  -DODBC_EXAMPLES=OFF \
  -DODBC_UNIT_TESTING=OFF \
  -DCLIENT_LIBRARY_INTEGRATION_TESTING=OFF

# -----------------------------------------
# Build (parallel, stable)
# -----------------------------------------
io::run cmake --build cmake-out --parallel "${JOBS}"

# -----------------------------------------
# Tests
# -----------------------------------------
mapfile -t ctest_args < <(ctest::common_args)
io::run env -C cmake-out ctest "${ctest_args[@]}"
