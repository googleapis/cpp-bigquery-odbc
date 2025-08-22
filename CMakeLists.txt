# ~~~
# Copyright 2023 Google Inc.
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
# Default version, can be overridden by -DPROJECT_VERSION=x.y.z
if (NOT DEFINED PROJECT_VERSION)
    set(PROJECT_VERSION "2.0.0")
endif ()

project(
    cpp-bigquery-odbc
    VERSION ${PROJECT_VERSION}
    LANGUAGES CXX)

set(PROJECT_VERSION_PRE_RELEASE "rc")

include(CTest)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

if (ENABLE_SANITIZER)
    if (UNIX)
        message(STATUS "Building with AddressSanitizer")

        # Add sanitizer flags to compiler
        set(CMAKE_C_FLAGS
            "${CMAKE_C_FLAGS} -O1 -fsanitize=address -fno-omit-frame-pointer -g"
        )
        set(CMAKE_CXX_FLAGS
            "${CMAKE_CXX_FLAGS} -O1 -fsanitize=address -fno-omit-frame-pointer -g"
        )

        # Add sanitizer flags to linker
        set(CMAKE_EXE_LINKER_FLAGS
            "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
        set(CMAKE_SHARED_LINKER_FLAGS
            "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
    else ()
        message(WARNING "AddressSanitizer is only supported on UNIX platforms")
    endif ()
endif ()

# If BQ_DRIVER_INTEGRATION_TESTS is enabled then we need to build the
# google_cloud_odbc_bq_driver as a shared library with
# CMAKE_POSITION_INDEPENDENT_CODE ON as static linking is not supported for ODBC
# drivers.
if (BQ_DRIVER_INTEGRATION_TESTS)
    set(BUILD_SHARED_LIBS ON)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif ()

set(FULL_VERSION "${PROJECT_VERSION}")
add_subdirectory(google/cloud/odbc)
