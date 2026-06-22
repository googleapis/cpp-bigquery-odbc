#!/usr/bin/env bash
#
# Copyright 2024 Google LLC
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

# Make our include guard clean against set -o nounset.
test -n "${CI_DEPENDENCIES_CLOUD_SDK_SH__:-}" || declare -i CI_DEPENDENCIES_CLOUD_SDK_SH__=0
if ((CI_DEPENDENCIES_CLOUD_SDK_SH__++ != 0)); then
  return 0
fi # include guard

readonly CPP_BIGQUERY_ODBC_CLOUD_SDK_VERSION="573.0.0"
declare -A -r CPP_BIGQUERY_ODBC_SDK_SHA256=(
  ["x86_64"]="cb5891435a561b5c614e8b06d603eb943db3b7d61ef682e48fbb6093faeed6e0"
  ["arm"]="aab2cd55c8a804cb15555fc1957d7e70a51135b81396e226a04d674deef36b0d"
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

export CLOUD_SDK_LOCATION=/usr/local/google-cloud-sdk
export PATH=${CLOUD_SDK_LOCATION}/bin:${PATH}
