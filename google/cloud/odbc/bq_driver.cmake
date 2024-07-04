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

# To avoid maintaining the list of files for the library, export them to a .bzl
# file.
if (NOT COMMAND create_bazel_config)
    include(CreateOdbcBazelConfig)
endif ()

# BQ Driver Internal Library
add_library(
    google_cloud_odbc_bq_driver_internal # cmake-format: sort
    bq_driver/internal/data_translation.cc
    bq_driver/internal/data_translation.h
    bq_driver/internal/diagnostics.cc
    bq_driver/internal/diagnostics.h
    bq_driver/internal/odbc_conn_attr.cc
    bq_driver/internal/odbc_conn_attr.h
    bq_driver/internal/odbc_conn_handle.cc
    bq_driver/internal/odbc_conn_handle.h
    bq_driver/internal/odbc_desc_attr.cc
    bq_driver/internal/odbc_desc_attr.h
    bq_driver/internal/odbc_desc_handle.cc
    bq_driver/internal/odbc_desc_handle.h
    bq_driver/internal/odbc_env_handle.cc
    bq_driver/internal/odbc_env_handle.h
    bq_driver/internal/odbc_handle.cc
    bq_driver/internal/odbc_handle.h
    bq_driver/internal/odbc_internal_commons.cc
    bq_driver/internal/odbc_internal_commons.h
    bq_driver/internal/odbc_query.cc
    bq_driver/internal/odbc_query.h
    bq_driver/internal/odbc_sql_fetch.cc
    bq_driver/internal/odbc_sql_fetch.h
    bq_driver/internal/odbc_sql_fns.cc
    bq_driver/internal/odbc_sql_fns.h
    bq_driver/internal/odbc_sql_foreign_keys.cc
    bq_driver/internal/odbc_sql_foreign_keys.h
    bq_driver/internal/odbc_sql_info.cc
    bq_driver/internal/odbc_sql_info.h
    bq_driver/internal/odbc_sql_primary_keys.cc
    bq_driver/internal/odbc_sql_primary_keys.h
    bq_driver/internal/odbc_sql_tables.cc
    bq_driver/internal/odbc_sql_tables.h
    bq_driver/internal/odbc_sql_type_info.cc
    bq_driver/internal/odbc_sql_type_info.h
    bq_driver/internal/odbc_stmt_attr.cc
    bq_driver/internal/odbc_stmt_attr.h
    bq_driver/internal/odbc_stmt_handle.cc
    bq_driver/internal/odbc_stmt_handle.h
    bq_driver/internal/odbc_transactions.cc
    bq_driver/internal/odbc_transactions.h
    bq_driver/internal/odbc_type_utils.cc
    bq_driver/internal/odbc_type_utils.h
    bq_driver/internal/trace_utils.cc
    bq_driver/internal/trace_utils.h
    bq_driver/internal/utils.cc
    bq_driver/internal/utils.h)

target_link_libraries(
    google_cloud_odbc_bq_driver_internal
    google-cloud-cpp::experimental-bigquery_rest # We need this dependency to
                                                 # use 'options' from client
                                                 # libraries
    odbc_bq_client_interface
    odbc_internal)
target_include_directories(google_cloud_odbc_bq_driver_internal
                           PUBLIC ${CMAKE_SOURCE_DIR})
target_include_directories(google_cloud_odbc_bq_driver_internal
                           PRIVATE $ENV{ODBC_INCLUDE_PATH})

create_bazel_config(google_cloud_odbc_bq_driver_internal YEAR 2023)

# BQ Driver Library
add_library(
    google_cloud_odbc_bq_driver # cmake-format: sort
    bq_driver/odbc_api.cc
    bq_driver/odbc_commons.cc
    bq_driver/odbc_commons.h
    bq_driver/odbc_connection.cc
    bq_driver/odbc_connection.h
    bq_driver/odbc_descriptor.cc
    bq_driver/odbc_descriptor.h
    bq_driver/odbc_diagnostics.cc
    bq_driver/odbc_diagnostics.h
    bq_driver/odbc_driver_metadata.cc
    bq_driver/odbc_driver_metadata.h
    bq_driver/odbc_environment.cc
    bq_driver/odbc_environment.h
    bq_driver/odbc_lock.cc
    bq_driver/odbc_lock.h
    bq_driver/odbc_sql_requests.cc
    bq_driver/odbc_sql_requests.h
    bq_driver/odbc_sql_results.cc
    bq_driver/odbc_sql_results.h
    bq_driver/odbc_statement.cc
    bq_driver/odbc_statement.h
    bq_driver/odbc_trace.cc
    bq_driver/odbc_trace.h
    bq_driver/odbc_utils.cc
    bq_driver/odbc_utils.h)

target_include_directories(google_cloud_odbc_bq_driver PUBLIC ./)
target_include_directories(google_cloud_odbc_bq_driver
                           PRIVATE $ENV{ODBC_INCLUDE_PATH})
target_link_libraries(google_cloud_odbc_bq_driver
                      google_cloud_odbc_bq_driver_internal)

# target_link_libraries(google_cloud_odbc_bq_driver )
add_subdirectory(bq_client_interface)
add_subdirectory(internal)

target_compile_features(google_cloud_odbc_bq_driver PUBLIC cxx_std_17)
set_target_properties(
    google_cloud_odbc_bq_driver
    PROPERTIES EXPORT_NAME google-cloud-odbc::bq-driver
               VERSION "${PROJECT_VERSION}"
               SOVERSION "${PROJECT_VERSION_MAJOR}")

add_library(google-cloud-odbc::bq-driver ALIAS google_cloud_odbc_bq_driver)

create_bazel_config(google_cloud_odbc_bq_driver YEAR 2023)

# Function for running for unit tests
function (bq_driver_define_unit_tests)
    if (NOT ODBC_UNIT_TESTING)
        return()
    endif ()

    enable_testing()

    add_executable(
        google_cloud_odbc_bq_driver_unit_tests
        bq_driver/internal/data_translation_test.cc
        bq_driver/internal/diagnostics_test.cc
        bq_driver/internal/odbc_conn_attr_test.cc
        bq_driver/internal/odbc_conn_handle_test.cc
        bq_driver/internal/odbc_desc_attr_test.cc
        bq_driver/internal/odbc_desc_handle_test.cc
        bq_driver/internal/odbc_env_handle_test.cc
        bq_driver/internal/odbc_internal_commons_test.cc
        bq_driver/internal/odbc_sql_fetch_test.cc
        bq_driver/internal/odbc_sql_fns_test.cc
        bq_driver/internal/odbc_sql_foreign_keys_test.cc
        bq_driver/internal/odbc_sql_info_test.cc
        bq_driver/internal/odbc_sql_primary_keys_test.cc
        bq_driver/internal/odbc_sql_tables_test.cc
        bq_driver/internal/odbc_sql_type_info_test.cc
        bq_driver/internal/odbc_stmt_attr_test.cc
        bq_driver/internal/odbc_stmt_handle_test.cc
        bq_driver/internal/odbc_transactions_test.cc
        bq_driver/internal/odbc_type_utils_test.cc
        bq_driver/internal/trace_utils_test.cc
        bq_driver/internal/utils_test.cc
        bq_driver/odbc_commons_test.cc
        bq_driver/odbc_connection_test.cc
        bq_driver/odbc_descriptor_test.cc
        bq_driver/odbc_diagnostics_test.cc
        bq_driver/odbc_driver_metadata_test.cc
        bq_driver/odbc_environment_test.cc
        bq_driver/odbc_lock_test.cc
        bq_driver/odbc_sql_requests_test.cc
        bq_driver/odbc_sql_results_test.cc
        bq_driver/odbc_statement_test.cc
        bq_driver/odbc_utils_test.cc)

    target_link_libraries(
        google_cloud_odbc_bq_driver_unit_tests google_cloud_odbc_testing_utils
        google_cloud_odbc_testing_bq_driver_utils GTest::gtest_main)

    target_compile_features(google_cloud_odbc_bq_driver_unit_tests
                            PUBLIC cxx_std_17)

    include(GoogleTest)
    gtest_discover_tests(google_cloud_odbc_bq_driver_unit_tests)
endfunction ()

bq_driver_define_unit_tests()
