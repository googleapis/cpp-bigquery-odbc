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

cmake_minimum_required(VERSION 3.14)
project(
  cpp-bigquery-odbc-integration-testing
  LANGUAGES CXX
)

function (define_integration_tests)
  if (NOT CLIENT_LIBRARY_INTEGRATION_TESTING)
    return()
  endif ()

  file(
      GLOB client_library_tests_cc
      RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
      "apis/*.cc"
  )
  add_executable(
      scheduled_integration_tests
      ${client_library_tests_cc}
  )

  add_subdirectory(${CMAKE_SOURCE_DIR}/google/cloud/odbc/testing_util odbc_testing_util)
  add_subdirectory(testing_util)

  target_link_libraries(
      scheduled_integration_tests PUBLIC
      google-cloud-cpp::experimental-bigquery_rest
      cpp_bigquery_odbc_integration_testing_util
      odbc_bigquery_testing_util
      GTest::gtest_main
  )

  target_include_directories(scheduled_integration_tests PUBLIC ${CMAKE_SOURCE_DIR})

  target_compile_features(scheduled_integration_tests PUBLIC cxx_std_17)

  option(USER_ACCOUNT_AUTH "Run tests using User Authentication" OFF)
  if (USER_ACCOUNT_AUTH)
      add_definitions(-DUSER_ACCOUNT_AUTH)
  endif()

  include(GoogleTest)
  gtest_discover_tests(scheduled_integration_tests)
endfunction ()

define_integration_tests()
