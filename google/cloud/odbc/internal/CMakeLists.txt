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

if (NOT COMMAND create_bazel_config)
    include(CreateOdbcBazelConfig)
endif ()

add_library(
    odbc_internal STATIC
    diagnostic_records.h odbc_includes.h sql_state_constants.h
    status_record_or.cc status_record_or.h version.h)

target_include_directories(odbc_internal PRIVATE $ENV{ODBC_INCLUDE_PATH})
target_include_directories(odbc_internal PUBLIC ${CMAKE_SOURCE_DIR})

target_link_libraries(odbc_internal
                      google-cloud-cpp::experimental-bigquery_rest)

target_compile_features(odbc_internal PUBLIC cxx_std_17)

create_bazel_config(odbc_internal YEAR 2024)

# version.h will be generated from version.h.in into build dir
set(FULL_VERSION
    "${FULL_VERSION}"
    CACHE STRING "Full version")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
               ${CMAKE_CURRENT_SOURCE_DIR}/version.h @ONLY)

# Convert version to numeric form for Windows RC
string(REPLACE "." "," DRIVER_VERSION_LIST "${FULL_VERSION}")

# version.rc will be generated from version.rc.in into build dir
set(DRIVER_VERSION_RC "${DRIVER_VERSION_LIST}")
set(DRIVER_VERSION "${FULL_VERSION}")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.rc.in
               ${CMAKE_CURRENT_SOURCE_DIR}/version.rc @ONLY)

if (WIN32)
    find_program(CMAKE_RC_COMPILER NAMES rc.exe windres)
    if (CMAKE_RC_COMPILER)
        enable_language(RC)
        message(STATUS "Using RC Compiler: ${CMAKE_RC_COMPILER}")
        target_sources(odbc_internal
                       PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/version.rc)

        set_source_files_properties(version.rc PROPERTIES LANGUAGE RC)
    else ()
        message(FATAL_ERROR "No resource compiler found (rc.exe or windres)")
    endif ()
endif ()

# Function for running unit tests
function (odbc_internal_define_unit_tests)
    if (NOT ODBC_UNIT_TESTING)
        return()
    endif ()

    enable_testing()

    file(
        GLOB odbc_internal_unit_tests_cc
        RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
        "*_test.cc")

    add_executable(odbc_internal_unit_tests ${odbc_internal_unit_tests_cc})

    target_link_libraries(odbc_internal_unit_tests odbc_internal
                          GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(odbc_internal_unit_tests)
endfunction ()

odbc_internal_define_unit_tests()
