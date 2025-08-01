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
project(cpp-bigquery-odbc-bq-driver LANGUAGES CXX)

include(FetchContent)

# Store the original BUILD_SHARED_LIBS value
set(ORIGINAL_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})

# Set BUILD_SHARED_LIBS to OFF for these specific dependencies
set(BUILD_SHARED_LIBS OFF)

# We need googletest for all our tests target.
find_package(GTest CONFIG QUIET)
if (NOT GTest_FOUND)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/v1.13.0.tar.gz)
    # For Windows: Prevent overriding the parent project's compiler/linker
    # settings
    set(gtest_force_shared_crt
        ON
        CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
else ()
    find_package(GTest CONFIG REQUIRED)
endif ()

# Force static linking for Abseil specifically
set(ABSL_PROPAGATE_CXX_STD ON)
set(ABSL_ENABLE_INSTALL OFF)
if(WIN32)
    set(ABSL_BUILD_DLL OFF)  # Critical for Windows static builds
endif()
find_package(absl CONFIG REQUIRED)

# Fetching dependencies from google-cloud-cpp
find_package(google_cloud_cpp_bigquery_rest CONFIG QUIET)
if (NOT google_cloud_cpp_bigquery_rest_FOUND OR NOT
                                                google_cloud_cpp_common_FOUND)
    # These flags need to be set to build google_cloud_cpp_bigquery_rest without
    # examples
    set(GOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND ON)
    set(GOOGLE_CLOUD_CPP_ENABLE_EXAMPLES OFF)
    set(BUILD_TESTING OFF)
    set(GOOGLE_CLOUD_CPP_ENABLE experimental-bigquery_rest oauth2 bigquery
                                resourcemanager serviceusage)

    FetchContent_Declare(
        google-cloud-cpp
        URL https://github.com/googleapis/google-cloud-cpp/archive/refs/heads/main.zip
    )
    FetchContent_MakeAvailable(google-cloud-cpp)
else ()
    find_package(google_cloud_cpp_bigquery CONFIG REQUIRED)
    find_package(google_cloud_cpp_bigquery_rest CONFIG REQUIRED)
    find_package(google_cloud_cpp_oauth2 CONFIG REQUIRED)
    find_package(google_cloud_cpp_resourcemanager REQUIRED)
    find_package(google_cloud_cpp_serviceusage REQUIRED)
endif ()

# Restore the original BUILD_SHARED_LIBS value
set(BUILD_SHARED_LIBS ${ORIGINAL_BUILD_SHARED_LIBS})

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/testing)

include(bq_driver.cmake)

add_subdirectory(integration_tests)
