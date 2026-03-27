#!/bin/bash
#
# Copyright 2023 Google LLC
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

export CC=clang
export CXX=clang++

WRAPPER="${PWD}/clang-tidy-wrapper.sh"

cat >"${WRAPPER}" <<'EOF'
#!/bin/bash
set -e

# Find actual source file (.cc/.cpp/.h)
FILE=""
for arg in "$@"; do
  if [[ "$arg" == *.cc || "$arg" == *.cpp || "$arg" == *.h ]]; then
    FILE="$arg"
    break
  fi
done

# If no source file found, skip
if [[ -z "$FILE" ]]; then
  echo "Skipping clang-tidy (no source file detected)"
  exit 0
fi

# Get the absolute path (resolve symlinks if any)
FILE=$(realpath "$FILE" 2>/dev/null || echo "$FILE")

# Skip external / third-party files
if [[ "$FILE" == *"/_deps/"* ]] || \
   [[ "$FILE" == *"/external/"* ]] || \
   [[ "$FILE" == *"/cmake-out/"* ]] || \
   [[ "$FILE" == *"/googletest"* ]] || \
   [[ "$FILE" == *"/absl"* ]] || \
   [[ "$FILE" == *"/build/"* ]] || \
   [[ "$FILE" == *"/CMakeFiles/"* ]]; then
  echo "Skipping clang-tidy for external file: $FILE"
  exit 0
fi

# Run ONLY on your code
if [[ "$FILE" == */google/cloud/odbc/* ]]; then
  exec /usr/bin/clang-tidy \
    -header-filter="^./google/cloud/odbc/.*" \
    "$@"
else
  echo "Skipping clang-tidy for $FILE"
  exit 0
fi
EOF

chmod +x "${WRAPPER}"

mapfile -t cmake_args < <(cmake::common_args)

# See https://github.com/matus-chochlik/ctcache for docs about the clang-tidy-cache
# Note: we use C++14 for this build because we don't want tidy suggestions that
# require a newer C++ standard.
io::run cmake "${cmake_args[@]}" \
  -DCMAKE_CXX_CLANG_TIDY="${WRAPPER}" \
  -DCMAKE_CXX_STANDARD=17 \
  -DODBC_INTEGRATION_TESTING=OFF \
  -DODBC_UNIT_TESTING=ON \
  -DNO_ARROW=1 # -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
io::run cmake --build cmake-out

if [[ "${TRIGGER_TYPE}" != "manual" ]]; then
  # This build should fail if any of the above work generated
  # code differences (for example, by updating .bzl files).
  io::log_h2 "Highlight generated code differences"
  git diff --exit-code
fi

if [[ "${TRIGGER_TYPE}" != "manual" || "${VERBOSE_FLAG}" == "true" ]]; then
  io::log_h2 "ctcache stats"
  printf "%s: %s\n" "total size" "$(du -sh "${CTCACHE_DIR}")"
  printf "%s: %s\n" " num files" "$(find "${CTCACHE_DIR}" | wc -l)"
  echo
fi
