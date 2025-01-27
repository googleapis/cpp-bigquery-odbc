#!/usr/bin/env bash
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

bash --version

declare -A AVAILABLE_DEPENDENCIES
AVAILABLE_DEPENDENCIES[BAZEL]=ci/dependencies/bazel.sh
AVAILABLE_DEPENDENCIES[DRIVER_MANAGER_SETUP]=ci/dependencies/driver-manager-setup.sh
AVAILABLE_DEPENDENCIES[GCLOUD_SDK]=ci/dependencies/cloud-sdk.sh
AVAILABLE_DEPENDENCIES[iODBC]=ci/dependencies/iODBC.sh
AVAILABLE_DEPENDENCIES[unixODBC]=ci/dependencies/unixODBC.sh
AVAILABLE_DEPENDENCIES[DRIVER_MANAGER_SETUP_GOOGLE_DRIVER]=ci/dependencies/driver-manager-setup-google-driver.sh

DEPENDENCIES=${DEPENDENCIES:-}
echo "DEPENDENCIES::${DEPENDENCIES}"

IFS=',' read -ra dependencies_list <<<"$DEPENDENCIES"
for dependency in "${dependencies_list[@]}"; do
  dependency=$(echo "$dependency" | tr -d '[:space:]')
  if [[ -n ${dependency} && -n ${AVAILABLE_DEPENDENCIES[$dependency]} ]]; then
    echo "sourcing: ${AVAILABLE_DEPENDENCIES[${dependency}]}"
    source "${AVAILABLE_DEPENDENCIES[${dependency}]}"
  else
    echo "dependency:${dependency} is not available"
  fi
done
