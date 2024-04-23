// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_DESCRIPTOR_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_DESCRIPTOR_H

///////////////////////////////////////////////////////////
// Defines the following internal APIs related to
// ODBC descriptor:
//
// SQLAllocDescriptorInternal
// SQLGetDescFieldInternal
// SQLSetDescFieldInternal
// SQLGetDescRecInternal
// SQLSetDescRecInternal
// SQLCopyDescInternal
///////////////////////////////////////////////////////////

#include "google/cloud/odbc/bq_driver/internal/odbc_desc_handle.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver {

SQLRETURN SQLAllocDescHandle(SQLHANDLE in_handle, SQLHANDLE* out_desc_handle);

google::cloud::odbc_internal::StatusRecord SetDescField(
    google::cloud::odbc_bq_driver_internal::DescriptorHandle* descriptor_handle,
    SQLSMALLINT rec_number, SQLSMALLINT field_identifier, SQLPOINTER desc_value,
    SQLINTEGER desc_value_buffer_len);

SQLRETURN SQLSetDescFieldInternal(SQLHDESC descriptor_handle,
                                  SQLSMALLINT rec_number,
                                  SQLSMALLINT field_identifier,
                                  SQLPOINTER desc_value,
                                  SQLINTEGER desc_value_buffer_len);

SQLRETURN GetDescField(
    google::cloud::odbc_bq_driver_internal::DescriptorHandle* handle,
    SQLSMALLINT rec_number, SQLSMALLINT field_identifier, SQLPOINTER out_value,
    SQLINTEGER value_buffer_len, SQLINTEGER* value_string_len);

SQLRETURN SQLGetDescFieldInternal(SQLHDESC descriptor_handle,
                                  SQLSMALLINT rec_number,
                                  SQLSMALLINT field_identifier,
                                  SQLPOINTER out_value,
                                  SQLINTEGER value_buffer_len,
                                  SQLINTEGER* value_string_len);

SQLRETURN SetDescRec(
    google::cloud::odbc_bq_driver_internal::DescriptorHandle* handle,
    SQLSMALLINT rec_number, SQLSMALLINT type, SQLSMALLINT sub_type,
    SQLLEN length, SQLSMALLINT precision, SQLSMALLINT scale,
    SQLPOINTER data_ptr, SQLLEN* string_length_ptr, SQLLEN* indicator_ptr);

SQLRETURN SQLSetDescRecInternal(SQLHDESC descriptor_handle,
                                SQLSMALLINT rec_number, SQLSMALLINT type,
                                SQLSMALLINT sub_type, SQLLEN length,
                                SQLSMALLINT precision, SQLSMALLINT scale,
                                SQLPOINTER data_ptr, SQLLEN* string_length_ptr,
                                SQLLEN* indicator_ptr);

SQLRETURN GetDescRec(
    google::cloud::odbc_bq_driver_internal::DescriptorHandle* descriptor_handle,
    SQLSMALLINT rec_number, SQLCHAR* name, SQLSMALLINT buffer_length,
    SQLSMALLINT* string_length_ptr, SQLSMALLINT* type_ptr,
    SQLSMALLINT* sub_type_ptr, SQLLEN* length_ptr, SQLSMALLINT* precision_ptr,
    SQLSMALLINT* scale_ptr, SQLSMALLINT* nullable_ptr);

SQLRETURN SQLGetDescRecInternal(
    SQLHDESC descriptor_handle, SQLSMALLINT rec_number, SQLCHAR* name,
    SQLSMALLINT buffer_length, SQLSMALLINT* string_length_ptr,
    SQLSMALLINT* type_ptr, SQLSMALLINT* sub_type_ptr, SQLLEN* length_ptr,
    SQLSMALLINT* precision_ptr, SQLSMALLINT* scale_ptr,
    SQLSMALLINT* nullable_ptr);

SQLRETURN SQLCopyDescInternal(SQLHDESC source_desc_handle,
                              SQLHDESC target_desc_handle);

}  // namespace google::cloud::odbc_bq_driver

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_DESCRIPTOR_H
