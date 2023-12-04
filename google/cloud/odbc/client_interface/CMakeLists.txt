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

add_library(
    odbc_client_interface
    odbc_authentication.h
    odbc_authentication.cc
    odbc_bq_client.h
    odbc_bq_client.cc)

target_link_libraries(
    odbc_client_interface
    google_cloud_cpp_bigquery_rest
    google_cloud_odbc_bq_driver_internal
    stdc++fs)

target_include_directories(odbc_client_interface PUBLIC ${CMAKE_SOURCE_DIR})

target_compile_features(odbc_client_interface PUBLIC cxx_std_17)

create_bazel_config(odbc_client_interface YEAR 2023)

# Adding default BQ secrets during compilation
option(DEFAULT_BIGQUERY_CLIENT_ID "Default BigQuery ClientId" OFF)
if (DEFAULT_BIGQUERY_CLIENT_ID)
    add_compile_definitions(-DDEFAULT_BIGQUERY_CLIENT_ID="${DEFAULT_BIGQUERY_CLIENT_ID}")
endif()
option(DEFAULT_BIGQUERY_CLIENT_SECRET "Default BigQuery ClientSecret" OFF)
if (DEFAULT_BIGQUERY_CLIENT_SECRET)
    add_compile_definitions(-DDEFAULT_BIGQUERY_CLIENT_SECRET="${DEFAULT_BIGQUERY_CLIENT_SECRET}")
endif()

# Function for running unit tests
function (client_interface_define_unit_tests)
  if (NOT ODBC_UNIT_TESTING)
    return()
  endif ()

  enable_testing()

  add_executable(
      odbc_client_interface_unit_tests
      odbc_authentication_test.cc
      odbc_bq_client_test.cc
  )

  target_link_libraries(
      odbc_client_interface_unit_tests
      odbc_client_interface
      google_cloud_cpp_bigquery_rest
      GTest::gtest_main)

  include(GoogleTest)
  gtest_discover_tests(odbc_client_interface_unit_tests)
endfunction ()

client_interface_define_unit_tests()
