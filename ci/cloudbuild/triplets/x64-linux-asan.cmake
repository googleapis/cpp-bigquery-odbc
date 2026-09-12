# Copyright 2026 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

# Keep ASan-instrumented dependencies separate from the normal x64-linux
# binaries in vcpkg's binary cache.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_C_FLAGS "-O1 -fsanitize=address -fno-omit-frame-pointer -g")
set(VCPKG_CXX_FLAGS "-O1 -fsanitize=address -fno-omit-frame-pointer -g")
set(VCPKG_LINKER_FLAGS "-fsanitize=address")
