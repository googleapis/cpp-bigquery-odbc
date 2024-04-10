// Copyright 2024 Google LLC
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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_DESC_ATTR_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_DESC_ATTR_H

#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver_internal {

inline constexpr SQLINTEGER kNumPrecRadixForNonNumeric = 0;
inline constexpr SQLINTEGER kNumPrecRadixForApproximateNumeric = 2;
inline constexpr SQLINTEGER kNumPrecRadixForExactNumeric = 10;
inline constexpr SQLSMALLINT kDefaultIntervalPrecision = 0;
inline constexpr SQLSMALLINT kDefaultIntervalSecondsPrecision = 6;

struct Interval {
  SQLSMALLINT concise_sql_type;
  SQLSMALLINT concise_c_type;
  SQLSMALLINT datetime_interval_code;
};

enum class DescriptorType { kApplication, kIRD, kIPD, kARD, kAPD };

struct HeaderRecord {
  explicit HeaderRecord(SQLSMALLINT alloc_type) : alloc_type_(alloc_type){};

  [[nodiscard]] SQLSMALLINT GetAllocType() const { return alloc_type_; }

  void CopyHeaderRecordsFrom(HeaderRecord const& header_record);

  SQLULEN array_size = 0;
  SQLUSMALLINT* array_status_ptr = nullptr;
  SQLLEN* bind_offset_ptr = nullptr;
  SQLINTEGER bind_type = SQL_BIND_BY_COLUMN;
  SQLSMALLINT count = 0;
  SQLULEN* rows_processed_ptr = nullptr;

 private:
  SQLSMALLINT alloc_type_ = SQL_DESC_ALLOC_AUTO;
};

struct DescriptorRecord {
  void SetName(std::string const& val, SQLINTEGER buffer_len);
  odbc_internal::StatusRecord SetNumPrecRadix(SQLINTEGER value);
  odbc_internal::StatusRecord SetParameterType(SQLSMALLINT value);
  odbc_internal::StatusRecord SetUnnamed(SQLSMALLINT value);
  odbc_internal::StatusRecord SetType(SQLSMALLINT value,
                                      DescriptorType desc_type);
  odbc_internal::StatusRecord SetConciseType(SQLSMALLINT value);
  odbc_internal::StatusRecord SetDataPointer(SQLPOINTER data_ptr,
                                             DescriptorType const& desc_type);
  [[nodiscard]] odbc_internal::StatusRecord ConsistencyCheck() const;

  SQLINTEGER auto_unique_value = SQL_FALSE;
  std::string base_column_name;
  std::string base_table_name;
  SQLINTEGER case_sensitive = SQL_TRUE;
  std::string catalog_name;
  SQLSMALLINT concise_type = SQL_C_DEFAULT;
  SQLPOINTER data_ptr = nullptr;
  SQLSMALLINT datetime_interval_code = 0;
  SQLINTEGER datetime_interval_precision = 2;
  SQLLEN display_size = 16384;
  SQLSMALLINT fixed_prec_scale = SQL_FALSE;
  SQLLEN* indicator_ptr = nullptr;
  std::string label;
  SQLULEN length = 0;
  std::string literal_prefix;
  std::string literal_suffix;
  std::string local_type_name;
  std::string name;
  SQLSMALLINT nullable = SQL_NULLABLE;
  SQLINTEGER num_prec_radix = 0;
  SQLLEN octet_length = 0;
  SQLLEN* octet_length_ptr = nullptr;
  SQLSMALLINT parameter_type = SQL_PARAM_INPUT;
  SQLSMALLINT precision = 0;
  SQLSMALLINT rowver = SQL_FALSE;
  SQLSMALLINT scale = 0;
  std::string schema_name;
  SQLSMALLINT searchable = SQL_PRED_SEARCHABLE;
  std::string table_name;
  SQLSMALLINT type = SQL_C_DEFAULT;
  std::string type_name;
  SQLSMALLINT unnamed = SQL_NAMED;
  SQLSMALLINT sql_desc_unsigned = SQL_TRUE;
  SQLSMALLINT updatable = SQL_ATTR_READONLY;

 private:
  void SetIntervalType(Interval const& entry, DescriptorType desc_type);
  void SetDatetimeType(Interval const& entry, DescriptorType desc_type);
  odbc_internal::StatusRecord SetOtherType(SQLSMALLINT value,
                                           std::string const& error_message);
  [[nodiscard]] bool IsTypeValid(SQLSMALLINT valid_type,
                                 SQLSMALLINT valid_concise_type,
                                 SQLSMALLINT valid_code) const;
  [[nodiscard]] bool IsTypeValid(SQLSMALLINT valid_type,
                                 Interval const& interval) const;
};

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_DESC_ATTR_H
