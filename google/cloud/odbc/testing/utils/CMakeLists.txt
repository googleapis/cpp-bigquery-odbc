# ~~~
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
# ~~~

if (NOT ODBC_INTEGRATION_TESTING
    AND NOT ODBC_UNIT_TESTING
    AND NOT CLIENT_LIBRARY_INTEGRATION_TESTING)
    return()
endif ()

if (NOT COMMAND create_bazel_config)
    include(CreateOdbcBazelConfig)
endif ()

add_library(google_cloud_odbc_testing_utils # cmake-format: sort
            env_vars.cc env_vars.h status_matchers.cc status_matchers.h)

target_link_libraries(google_cloud_odbc_testing_utils
                      google-cloud-cpp::experimental-bigquery_rest GTest::gtest_main GTest::gtest GTest::gmock)

create_bazel_config(google_cloud_odbc_testing_utils YEAR 2024)
