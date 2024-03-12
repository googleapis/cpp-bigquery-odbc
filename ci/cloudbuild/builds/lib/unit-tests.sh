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
test -n "${CI_CLOUDBUILD_BUILDS_LIB_UNIT_TESTS_SH__:-}" || declare -i CI_CLOUDBUILD_BUILDS_LIB_UNIT_TESTS_SH__=0
if ((CI_CLOUDBUILD_BUILDS_LIB_UNIT_TESTS_SH__++ != 0)); then
  return 0
fi # include guard

# Outputs a list of Bazel arguments that should be used when running
# unit tests. These do not include the common `bazel::common_args`.
#
# Example usage:
#
#   mapfile -t args < <(bazel::common_args)
#   mapfile -t unit_tests_args < <(unit_tests::bazel_args)
#   bazel test "${args[@]}" "${unit_tests_args[@]}"
#

export CPP_BIGQUERY_ODBC_DRIVER_TEST_DATA_PATH=${PROJECT_ROOT}/google/cloud/odbc/bq_driver/internal/test_data/

function unit_tests::bazel_args() {
  declare -a args

  args+=(
    "--test_env=CPP_BIGQUERY_ODBC_DRIVER_TEST_DATA_PATH=${PROJECT_ROOT}/google/cloud/odbc/bq_driver/internal/test_data/"
    "--test_env=HOME=$HOME"
  )
  printf "%s\n" "${args[@]}"
}
