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
project(cpp-bigquery-odbc-integration-testing LANGUAGES CXX)

function (define_client_library_integration_tests)
    if (NOT CLIENT_LIBRARY_INTEGRATION_TESTING)
        return()
    endif ()

    file(
        GLOB client_library_tests_cc
        RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
        "bq_apis/*_test.cc")
    add_executable(scheduled_integration_tests ${client_library_tests_cc})

    target_link_libraries(
        scheduled_integration_tests
        PUBLIC odbc_bq_client_interface
               google_cloud_odbc_testing_client_library_utils
               google_cloud_odbc_testing_utils GTest::gtest_main)

    target_include_directories(scheduled_integration_tests
                               PUBLIC ${CMAKE_SOURCE_DIR})

    target_compile_features(scheduled_integration_tests PUBLIC cxx_std_17)

    option(USER_ACCOUNT_AUTH "Run tests using User Authentication" OFF)
    if (USER_ACCOUNT_AUTH)
        add_definitions(-DUSER_ACCOUNT_AUTH)
    endif ()

    option(EXTERNAL_ACCOUNT_AUTH "Run tests using External Authentication" OFF)
    if (EXTERNAL_ACCOUNT_AUTH)
        add_definitions(-DEXTERNAL_ACCOUNT_AUTH)
    endif ()

    include(GoogleTest)
    gtest_discover_tests(scheduled_integration_tests)
endfunction ()

function (odbc_define_tests)
    if (NOT ODBC_INTEGRATION_TESTING)
        return()
    endif ()

    enable_testing()

    file(
        GLOB odbc_driver_tests_src_cc
        RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
        "odbc_driver_tests/*_test.cc")

    set(odbc_driver_tests_src ${odbc_driver_tests_src_cc})

    # Setting BQ_DRIVER_INTEGRATION_TESTS runs tests only for Google Driver
    if (BQ_DRIVER_INTEGRATION_TESTS)
        add_definitions(-DBQ_DRIVER_INTEGRATION_TESTS)
    endif ()

    add_executable(all_tests ${odbc_driver_tests_src})

    target_link_libraries(all_tests google_cloud_odbc_testing_odbc_utils
                          GTest::gtest_main)

    target_include_directories(all_tests PUBLIC ${CMAKE_SOURCE_DIR})
    target_include_directories(all_tests PRIVATE $ENV{ODBC_INCLUDE_PATH})

    target_compile_features(all_tests PUBLIC cxx_std_17)

    include(GoogleTest)
    gtest_discover_tests(all_tests)
endfunction ()

function (odbc_define_demo_tests)
    if (NOT ODBC_DEMO_TESTING)
        return()
    endif ()

    enable_testing()

    add_executable(
        all_demo_tests
        odbc_driver_tests/demo/connection_demo_test.cc
        odbc_driver_tests/demo/foreign_keys_demo_test.cc
        odbc_driver_tests/demo/primary_keys_demo_test.cc
        odbc_driver_tests/demo/query_execution_demo_test.cc)

    target_link_libraries(all_demo_tests google_cloud_odbc_testing_odbc_utils
                          google_cloud_odbc_bq_driver GTest::gtest_main)

    target_include_directories(all_demo_tests PUBLIC ${CMAKE_SOURCE_DIR})
    target_include_directories(all_demo_tests PRIVATE $ENV{ODBC_INCLUDE_PATH})

    target_compile_features(all_demo_tests PUBLIC cxx_std_17)
endfunction ()

# Isolated Performance Target for performance testing
add_executable(performance_test EXCLUDE_FROM_ALL
               odbc_driver_tests/examples/catalog_performance_example.cc)

target_link_libraries(
    performance_test PRIVATE google_cloud_odbc_testing_odbc_utils
                             GTest::gtest_main)

target_include_directories(performance_test PUBLIC ${CMAKE_SOURCE_DIR})
target_include_directories(performance_test PRIVATE $ENV{ODBC_INCLUDE_PATH})
target_compile_features(performance_test PUBLIC cxx_std_17)

function (odbc_define_examples)
    if (NOT ODBC_EXAMPLES)
        return()
    endif ()

    include(CppBigqueryOdbcCommon)

    # Adding files for non-windows
    file(
        GLOB examples_cc
        RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
        "odbc_driver_tests/examples/*.cc")

    # Adding files for windows
    if (WIN32)
        file(
            GLOB windows_examples_cc
            RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
            "odbc_driver_tests/examples/windows/*.cc")

        list(APPEND examples_cc ${windows_examples_cc})
    endif ()

    foreach (fname IN LISTS examples_cc)
        cpp_bigquery_odbc_add_executable(target "example" "${fname}")
        target_link_libraries(${target} google_cloud_odbc_testing_odbc_utils
                              GTest::gtest)
        # Setting BQ_DRIVER_INTEGRATION_TESTS should use
        # google_cloud_odbc_bq_driver target
        if (BQ_DRIVER_INTEGRATION_TESTS)
            add_definitions(-DBQ_DRIVER_INTEGRATION_TESTS)
        endif ()
        if (WIN32)
            target_link_libraries(${target} odbc32)
        else ()
            target_link_libraries(${target} odbc)
        endif ()
        target_include_directories(${target} PUBLIC ${CMAKE_SOURCE_DIR})
        target_include_directories(${target} PRIVATE $ENV{ODBC_INCLUDE_PATH})
        target_compile_features(${target} PUBLIC cxx_std_17)
    endforeach ()
endfunction ()

odbc_define_demo_tests()

odbc_define_examples()

odbc_define_tests()

define_client_library_integration_tests()
