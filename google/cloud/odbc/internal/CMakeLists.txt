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
endif()

add_library(
    odbc_internal
    odbc_includes.h
    diagnostic_records.h
    sql_state_constants.h
    status_record_or.h
    status_record_or.cc
)

target_include_directories(odbc_internal PRIVATE $ENV{ODBC_INCLUDE_PATH})
target_include_directories(odbc_internal PUBLIC ${CMAKE_SOURCE_DIR})

target_link_libraries(
    odbc_internal
    absl::strings
)

target_compile_features(odbc_internal PUBLIC cxx_std_17)

create_bazel_config(odbc_internal YEAR 2024)

# Function for running unit tests
function (odbc_internal_define_unit_tests)
    if (NOT ODBC_UNIT_TESTING)
        return()
    endif ()

    enable_testing()

    file(
        GLOB odbc_internal_unit_tests_cc
        RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
        "*_test.cc"
    )

    add_executable(
        odbc_internal_unit_tests
        ${odbc_internal_unit_tests_cc}
    )

    target_link_libraries(
        odbc_internal_unit_tests
        odbc_internal
        GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(odbc_internal_unit_tests)
endfunction ()

odbc_internal_define_unit_tests()
