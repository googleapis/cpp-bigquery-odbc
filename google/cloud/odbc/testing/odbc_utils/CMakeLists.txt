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

add_library(
    google_cloud_odbc_testing_odbc_utils # cmake-format: sort
    catalog.cc
    catalog.h
    commons.cc
    commons.h
    connection.cc
    connection.h
    properties.cc
    properties.h
    statement.cc
    statement.h)

target_include_directories(google_cloud_odbc_testing_odbc_utils
                           PUBLIC ${CMAKE_SOURCE_DIR})

# Setting BQ_DRIVER_INTEGRATION_TESTS should use google_cloud_odbc_bq_driver
# target
if (BQ_DRIVER_INTEGRATION_TESTS)
    add_definitions(-DBQ_DRIVER_INTEGRATION_TESTS)
    target_link_libraries(google_cloud_odbc_testing_odbc_utils
                          google_cloud_odbc_bq_driver)
else ()
    target_link_libraries(google_cloud_odbc_testing_odbc_utils odbc)
endif ()

create_bazel_config(google_cloud_odbc_testing_odbc_utils YEAR 2024)
