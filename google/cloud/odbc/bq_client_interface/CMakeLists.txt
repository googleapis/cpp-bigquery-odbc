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
    odbc_bq_client_interface
    jobs.h
    jobs.cc
    projects.h
    projects.cc
    odbc_authentication.h
    odbc_authentication.cc
    odbc_bq_client.h
    odbc_bq_client.cc
    setenv.h
    setenv.cc)

target_link_libraries(
    odbc_bq_client_interface
    google-cloud-cpp::oauth2
    google-cloud-cpp::experimental-bigquery_rest)

target_include_directories(odbc_bq_client_interface PUBLIC ${CMAKE_SOURCE_DIR})

target_compile_features(odbc_bq_client_interface PUBLIC cxx_std_17)

create_bazel_config(odbc_bq_client_interface YEAR 2023)

# Function for running unit tests
function (client_interface_define_unit_tests)
  if (NOT ODBC_UNIT_TESTING)
    return()
  endif ()

  enable_testing()

  file(
      GLOB bq_client_interface_unit_tests_cc
      RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
      "*_test.cc"
  )

  add_executable(
      odbc_bq_client_interface_unit_tests
      ${bq_client_interface_unit_tests_cc}
  )

  target_link_libraries(
      odbc_bq_client_interface_unit_tests
      odbc_bq_client_interface
      google_cloud_odbc_testing_utils
      GTest::gtest_main)

  include(GoogleTest)
  gtest_discover_tests(odbc_bq_client_interface_unit_tests)
endfunction ()

client_interface_define_unit_tests()
