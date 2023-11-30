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
  cpp-bigquery-odbc-bq-driver
  LANGUAGES CXX
)

include(FetchContent)

# We need googletest for all our tests target.
find_package(GTest CONFIG QUIET)
if (NOT GTest_FOUND)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/v1.13.0.tar.gz
  )
  # For Windows: Prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
else()
  find_package(GTest CONFIG REQUIRED)
endif()

function (odbc_define_tests)
  if (NOT ODBC_INTEGRATION_TESTING)
    return()
  endif ()

  enable_testing()

  add_executable(
    all_tests
    testing/commons.cc
    testing/connection.cc
    testing/driver_properties.cc
    testing/statement.cc
    testing/catalog.cc
    tests/connection_test.cc
    tests/driver_properties_test.cc
    tests/descriptor_test.cc
    tests/statement_test.cc
    tests/catalog_test.cc
  )

  target_link_libraries(
    all_tests
    odbc
    GTest::gtest_main
    GTest::gmock
  )

  target_include_directories(all_tests PUBLIC ./)
  target_include_directories(all_tests PRIVATE $ENV{ODBC_INCLUDE_PATH})

  target_compile_features(all_tests PUBLIC cxx_std_17)

  include(GoogleTest)
  gtest_discover_tests(all_tests)
endfunction ()

# Fetching dependecies from google-cloud-cpp
find_package(google_cloud_cpp_bigquery_rest CONFIG QUIET)
if (NOT google_cloud_cpp_bigquery_rest_FOUND OR NOT google_cloud_cpp_common_FOUND)
  # These flags need to be set to build google_cloud_cpp_bigquery_rest without examples
  set(GOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND ON)
  set(GOOGLE_CLOUD_CPP_ENABLE_EXAMPLES OFF)
  set(BUILD_TESTING OFF)
  set(GOOGLE_CLOUD_CPP_ENABLE experimental-bigquery_rest)

  FetchContent_Declare(
    google-cloud-cpp
    URL https://github.com/googleapis/google-cloud-cpp/archive/refs/heads/main.zip
  )
  FetchContent_MakeAvailable(google-cloud-cpp)
else()
  find_package(google_cloud_cpp_bigquery_rest CONFIG REQUIRED)
endif()

include(bq_driver.cmake)

add_library(odbc_bq_driver dummy.cc)
target_link_libraries(odbc_bq_driver PRIVATE google_cloud_odbc_bq_driver)

odbc_define_tests()

add_subdirectory(integration_tests)

