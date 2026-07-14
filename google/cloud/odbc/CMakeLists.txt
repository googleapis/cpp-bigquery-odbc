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

# Option to compile only the standalone performance benchmarking executable.
# Bypasses compilation of the main driver and all of its heavy dependencies
# (such as Abseil, Arrow, RE2, and google-cloud-cpp) that require vcpkg.
option(BUILD_PERFORMANCE_TEST_ONLY "Build only the performance test target" OFF)

if (BUILD_PERFORMANCE_TEST_ONLY)
    # We need googletest for our test target.
    find_package(GTest CONFIG QUIET)
    if (NOT GTest_FOUND)
        FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/v1.17.0.tar.gz)
        set(gtest_force_shared_crt
            ON
            CACHE BOOL "" FORCE)
        # Force GTest to build as static libraries to prevent missing DLLs at
        # runtime on Windows
        set(old_build_shared ${BUILD_SHARED_LIBS})
        set(BUILD_SHARED_LIBS OFF)
        FetchContent_MakeAvailable(googletest)
        set(BUILD_SHARED_LIBS ${old_build_shared})
    else ()
        find_package(GTest CONFIG REQUIRED)
    endif ()

    # We need nlohmann_json for our test target.
    find_package(nlohmann_json CONFIG QUIET)
    if (NOT nlohmann_json_FOUND)
        FetchContent_Declare(
            nlohmann_json
            URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.tar.gz
        )
        FetchContent_MakeAvailable(nlohmann_json)
    else ()
        find_package(nlohmann_json CONFIG REQUIRED)
    endif ()

    # Enable building odbc_utils static library
    set(ODBC_INTEGRATION_TESTING ON)

    add_subdirectory(testing/odbc_utils)
    add_subdirectory(integration_tests)
    return()
endif ()

if (MSVC)
    foreach (lang C CXX)
        set(CMAKE_${lang}_FLAGS_INIT "/MT")
        set(CMAKE_${lang}_FLAGS_DEBUG_INIT "/MTd")
        set(CMAKE_${lang}_FLAGS_RELEASE_INIT "/MT")
        set(CMAKE_${lang}_FLAGS_MINSIZEREL_INIT "/MT")
        set(CMAKE_${lang}_FLAGS_RELWITHDEBINFO_INIT "/MT")
    endforeach ()

    add_compile_options(
        "$<$<CONFIG:Debug>:/MTd>" "$<$<CONFIG:Release>:/MT>"
        "$<$<CONFIG:RelWithDebInfo>:/MT>" "$<$<CONFIG:MinSizeRel>:/MT>")
endif ()

# Store the original BUILD_SHARED_LIBS value
set(ORIGINAL_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})

# Set BUILD_SHARED_LIBS to OFF for these specific dependencies
set(BUILD_SHARED_LIBS OFF)

# we need to forces OpenTelemetry to use the C++17 Standard Library (STL)
# implementation for variants and optionals.
add_definitions(-DOPENTELEMETRY_STL_VERSION=2017)
# We need googletest for all our tests target.
find_package(GTest CONFIG QUIET)
if (NOT GTest_FOUND)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/v1.17.0.tar.gz)
    # For Windows: Prevent overriding the parent project's compiler/linker
    # settings
    set(gtest_force_shared_crt
        ON
        CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
else ()
    find_package(GTest CONFIG REQUIRED)
endif ()

find_package(absl CONFIG REQUIRED)

if (ODBC_UNIT_TESTING)
    FetchContent_Declare(
        fuzztest
        URL "https://github.com/google/fuzztest/archive/refs/tags/2026-02-19.tar.gz"
    )
    set(FUZZTEST_USE_EXTERNAL_ABSL
        ON
        CACHE BOOL "" FORCE)
    set(FUZZTEST_USE_EXTERNAL_RE2
        ON
        CACHE BOOL "" FORCE)
    set(FUZZTEST_USE_EXTERNAL_GTEST
        ON
        CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(fuzztest)
endif ()

# Arrow is not supported on 32-bit systems
if (CMAKE_SIZEOF_VOID_P EQUAL 8 AND NOT NO_ARROW)
    find_package(Arrow REQUIRED)
else ()
    add_compile_definitions(NO_ARROW=1)
endif ()

find_package(re2 CONFIG REQUIRED)

# Fetching dependencies from google-cloud-cpp
find_package(google_cloud_cpp_bigquery_rest CONFIG QUIET)
if (NOT google_cloud_cpp_bigquery_rest_FOUND OR NOT
                                                google_cloud_cpp_common_FOUND)
    # These flags need to be set to build google_cloud_cpp_bigquery_rest without
    # examples
    set(GOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND ON)
    set(GOOGLE_CLOUD_CPP_ENABLE_EXAMPLES OFF)
    set(BUILD_TESTING OFF)
    set(CMAKE_POSITION_INDEPENDENT_CODE=ON)
    set(GOOGLE_CLOUD_CPP_ENABLE experimental-bigquery_rest oauth2 bigquery
                                resourcemanager serviceusage)

    FetchContent_Declare(
        google-cloud-cpp
        URL https://github.com/googleapis/google-cloud-cpp/archive/85bfaff9038e0791c7f48a995253dc20c307ac78.tar.gz
        PATCH_COMMAND ${CMAKE_COMMAND} -P
                      ${CMAKE_SOURCE_DIR}/cmake/PatchGoogleCloudCpp.cmake)
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
