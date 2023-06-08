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
  cpp-bigquery-odbc
  VERSION 1.0.0
  LANGUAGES CXX
)

# GoogleTest requires at least C++14
set(CMAKE_CXX_STANDARD 17)

include_directories(BEFORE $ENV{IODBC_INCLUDE_PATH})

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

set(GOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND ON)
set(BUILD_TESTING OFF)

FetchContent_Declare(
 google_cloud_cpp
 URL https://github.com/googleapis/google-cloud-cpp/archive/refs/heads/main.zip
)
FetchContent_MakeAvailable(google_cloud_cpp)

enable_testing()

include_directories(BEFORE ./)

add_executable(
  all_tests
  testing/commons.cc
  testing/connection.cc
  testing/misc.cc
  testing/statement.cc
  testing/catalog.cc
  tests/connection_test.cc
  tests/misc_test.cc
  tests/statement_test.cc
  tests/catalog_test.cc
)
target_link_libraries(
  all_tests
  odbc
  GTest::gtest_main
  google_cloud_cpp_bigquery
)

include(GoogleTest)
gtest_discover_tests(all_tests)

