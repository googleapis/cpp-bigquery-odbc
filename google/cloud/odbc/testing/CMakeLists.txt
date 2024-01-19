# ~~~
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
# ~~~

if (NOT ODBC_INTEGRATION_TESTING AND NOT ODBC_UNIT_TESTING AND NOT CLIENT_LIBRARY_INTEGRATION_TESTING)
    return()
endif ()

if (NOT COMMAND create_bazel_config)
  include(CreateOdbcBazelConfig)
endif()

add_library(
    google_cloud_odbc_testing # cmake-format: sort
    driver_commons.cc
    driver_commons.h
    driver_connection.cc
    driver_connection.h
    driver_properties.cc
    driver_properties.h
    driver_statement.cc
    driver_statement.h
    driver_catalog.cc
    driver_catalog.h)

target_link_libraries(
    google_cloud_odbc_testing
    google-cloud-cpp::experimental-bigquery_rest
    GTest::gmock)

# Setting BQ_DRIVER_INTEGRATION_TESTS should use google_cloud_odbc_bq_driver target
if (BQ_DRIVER_INTEGRATION_TESTS)
    target_link_libraries(
        google_cloud_odbc_testing
        google_cloud_odbc_bq_driver)
else()
    target_link_libraries(
        google_cloud_odbc_testing
        odbc)
endif()

add_library(
    google_cloud_odbc_testing_utils # cmake-format: sort
    utils/status_matchers.cc
    utils/status_matchers.h)

target_link_libraries(
    google_cloud_odbc_testing_utils
    google-cloud-cpp::experimental-bigquery_rest
    GTest::gmock)

create_bazel_config(google_cloud_odbc_testing YEAR 2023)
create_bazel_config(google_cloud_odbc_testing_utils YEAR 2023)
