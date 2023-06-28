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

readonly CPP_BIGQUERY_ODBC_CLOUD_SDK_VERSION="428.0.0"
declare -A -r CPP_BIGQUERY_ODBC_SDK_SHA256=(
  ["x86_64"]="a665909d2ff9cd3a927d84670c5a8d11f0c5fbcda2540bbea44e0d6f77b82e27"
  ["arm"]="d3d7d8bdde1abc8e8279b813b3341d20fa3af1928268d97c61b0553a8e590124"
)

ARCH="$(uname -m)"
if [[ "${ARCH}" == "aarch64" ]]; then
  # The tarball uses this name
  ARCH="arm"
fi
readonly ARCH

components=(
  beta
)

readonly SITE="https://dl.google.com/dl/cloudsdk/channels/rapid/downloads"
readonly TARBALL="google-cloud-cli-${CPP_BIGQUERY_ODBC_CLOUD_SDK_VERSION}-linux-${ARCH}.tar.gz"

curl -L "${SITE}/${TARBALL}" -o "${TARBALL}"
echo "${CPP_BIGQUERY_ODBC_SDK_SHA256[${ARCH}]} ${TARBALL}" | sha256sum --check -
tar x -C /usr/local -f "${TARBALL}"
/usr/local/google-cloud-sdk/bin/gcloud --quiet components install \
  "${components[@]}"
