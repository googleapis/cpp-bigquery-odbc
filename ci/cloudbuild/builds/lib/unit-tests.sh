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

# This bash library has various helper functions for our unit tests.

# Make our include guard clean against set -o nounset.
test -n "${CI_CLOUDBUILD_BUILDS_LIB_INTEGRATION_SH__:-}" || declare -i CI_CLOUDBUILD_BUILDS_LIB_INTEGRATION_SH__=0
if ((CI_CLOUDBUILD_BUILDS_LIB_INTEGRATION_SH__++ != 0)); then
  return 0
fi # include guard

# Outputs a list of Bazel arguments that should be used when running
# unit tests. These do not include the common `bazel::common_args`.
#
# Example usage:
#
#   mapfile -t args < <(bazel::common_args)
#   mapfile -t integration_args < <(unit-tests::bazel_args)
#   bazel test "${args[@]}" "${integration_args[@]}"
#
function unit-tests::bazel_args() {
  declare -a args

  DEFAULT_BIGQUERY_CLIENT_ID=$(gcloud secrets versions access latest --secret=default_bigquery_client_id)
  DEFAULT_BIGQUERY_CLIENT_SECRET=$(gcloud secrets versions access latest --secret=default_bigquery_client_secret)
  args+=(
    "--test_env=CPP_BIGQUERY_ODBC_DRIVER_TEST_DATA_PATH=${PROJECT_ROOT}/google/cloud/odbc/bq_driver/internal/test_data/"
    "--copt=-DDEFAULT_BIGQUERY_CLIENT_ID=\"${DEFAULT_BIGQUERY_CLIENT_ID}\""
    "--copt=-DDEFAULT_BIGQUERY_CLIENT_SECRET=\"${DEFAULT_BIGQUERY_CLIENT_SECRET}\""
  )
  printf "%s\n" "${args[@]}"
}
