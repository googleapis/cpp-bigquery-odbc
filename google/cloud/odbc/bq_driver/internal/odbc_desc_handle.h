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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_DESC_HANDLE_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_DESC_HANDLE_H

#include "google/cloud/odbc/bq_driver/internal/odbc_handle.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include <map>

namespace google::cloud::odbc_bq_driver_internal {

inline constexpr SQLINTEGER kNumPrecRadixForNonNumeric = 0;
inline constexpr SQLINTEGER kNumPrecRadixForApproximateNumeric = 2;
inline constexpr SQLINTEGER kNumPrecRadixForExactNumeric = 10;

enum class DescriptorType { kApplication, kIRD, kIPD };

struct HeaderRecord {
  SQLSMALLINT alloc_type = SQL_DESC_ALLOC_AUTO;
  SQLULEN array_size = 0;
  SQLUSMALLINT* array_status_ptr = nullptr;
  SQLLEN* bind_offset_ptr = nullptr;
  SQLINTEGER bind_type = SQL_BIND_BY_COLUMN;
  SQLSMALLINT count = 0;
  SQLULEN* rows_processed_ptr = nullptr;
};

struct DescriptorRecord {
  void SetName(std::string const& val, SQLINTEGER buffer_len);
  odbc_internal::StatusRecord SetNumPrecRadix(SQLINTEGER value);
  odbc_internal::StatusRecord SetParameterType(SQLSMALLINT value);
  odbc_internal::StatusRecord SetUnnamed(SQLSMALLINT value);
  odbc_internal::StatusRecord SetType(SQLSMALLINT value,
                                      DescriptorType desc_type);
  odbc_internal::StatusRecord SetConciseType(SQLSMALLINT value);

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
  SQLULEN length = 16384;
  std::string literal_prefix;
  std::string literal_suffix;
  std::string local_type_name;
  std::string name;
  SQLSMALLINT nullable = SQL_NULLABLE;
  SQLINTEGER num_prec_radix = 0;
  SQLLEN octet_length = 16384;
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
  odbc_internal::StatusRecord SetOtherType(SQLSMALLINT value,
                                           std::string const& error_message);
};

class DescriptorHandle : public Handle {
 public:
  explicit DescriptorHandle(DescriptorType type = DescriptorType::kApplication)
      : type_(type){};
  ~DescriptorHandle() = default;

  DescriptorHandle(DescriptorHandle const&) = default;
  DescriptorHandle& operator=(DescriptorHandle const&) = default;
  DescriptorHandle(DescriptorHandle&&) = default;
  DescriptorHandle& operator=(DescriptorHandle&&) = default;

  DescriptorType GetType() { return type_; }

  HeaderRecord& GetHeaderRecord() { return header_record_; }

  bool HasDescriptorRecord(int index) {
    return descriptor_records_.count(index) > 0;
  }

  // Should be called after HasDescriptorRecord function.
  // Because we can't return StatusRecordOr of a reference.
  DescriptorRecord& GetDescriptorRecord(int index) {
    return descriptor_records_[index];
  }

  void BindNewDescriptorRecord(SQLSMALLINT index,
                               DescriptorRecord descriptor_record);

  odbc_internal::StatusRecordOr<DescriptorRecord> UnbindDescriptorRecord(
      int index);

  odbc_internal::StatusRecord UnbindAllDescriptorRecordsFrom(int index);

 private:
  DescriptorType type_;
  HeaderRecord header_record_;
  std::map<int, DescriptorRecord> descriptor_records_;
};

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_DESC_HANDLE_H
