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

add_library(
    google_cloud_odbc_bq_driver # cmake-format: sort
    bq_driver/odbc_api.cc
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
    bq_driver/odbc_includes.h
    bq_driver/odbc_lock.cc
    bq_driver/odbc_lock.h
    bq_driver/odbc_sql_requests.cc
    bq_driver/odbc_sql_requests.h
    bq_driver/odbc_sql_results.cc
    bq_driver/odbc_sql_results.h
    bq_driver/odbc_statement.cc
    bq_driver/odbc_statement.h
    bq_driver/odbc_trace.cc
    bq_driver/odbc_trace.h)

target_include_directories(
    google_cloud_odbc_bq_driver PUBLIC ./)

target_compile_features(google_cloud_odbc_bq_driver PUBLIC cxx_std_17)
set_target_properties(
    google_cloud_odbc_bq_driver
    PROPERTIES EXPORT_NAME google-cloud-odbc::bq-driver
               VERSION "${PROJECT_VERSION}"
               SOVERSION "${PROJECT_VERSION_MAJOR}")

add_library(google-cloud-odbc::bq-driver ALIAS
            google_cloud_odbc_bq_driver)

create_bazel_config(google_cloud_odbc_bq_driver YEAR 2023)


