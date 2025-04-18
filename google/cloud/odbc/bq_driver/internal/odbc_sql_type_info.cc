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

#include "google/cloud/odbc/bq_driver/internal/odbc_sql_type_info.h"

namespace google::cloud::odbc_bq_driver_internal {

DSRow CreateDSRowFromTypeInfo(TypeInfoRow const& type_info) {
  DSRow ds_row;

  DSValue type_name;
  StringToDSValue(type_info.type_name, type_name);
  ds_row.emplace_back(type_name);

  DSValue data_type;
  ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(type_info.data_type),
                                 data_type);
  ds_row.emplace_back(data_type);

  DSValue col_size;
  ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(type_info.col_size),
                                 col_size);
  ds_row.emplace_back(col_size);

  if (type_info.literal_prefix) {
    DSValue literal_prefix;
    StringToDSValue(type_info.literal_prefix, literal_prefix);
    ds_row.emplace_back(literal_prefix);
  } else {
    ds_row.emplace_back();
  }

  if (type_info.literal_suffix) {
    DSValue literal_suffix;
    StringToDSValue(type_info.literal_suffix, literal_suffix);
    ds_row.emplace_back(literal_suffix);
  } else {
    ds_row.emplace_back();
  }

  if (type_info.create_params) {
    DSValue create_params;
    StringToDSValue(type_info.create_params, create_params);
    ds_row.emplace_back(create_params);
  } else {
    ds_row.emplace_back();
  }

  DSValue nullable;
  ArithmeticToDSValue<SQLBIGINT>(type_info.nullable, nullable);
  ds_row.emplace_back(nullable);

  DSValue case_sensitive;
  ArithmeticToDSValue<SQLBIGINT>(type_info.case_sensitive, case_sensitive);
  ds_row.emplace_back(case_sensitive);

  DSValue searchable;
  ArithmeticToDSValue<SQLBIGINT>(type_info.searchable, searchable);
  ds_row.emplace_back(searchable);

  DSValue unsigned_attribute;
  ArithmeticToDSValue<SQLBIGINT>(type_info.unsigned_attribute,
                                 unsigned_attribute);
  ds_row.emplace_back(unsigned_attribute);

  DSValue fixed_prec_scale;
  ArithmeticToDSValue<SQLBIGINT>(type_info.fixed_prec_scale, fixed_prec_scale);
  ds_row.emplace_back(fixed_prec_scale);

  DSValue auto_unique_value;
  ArithmeticToDSValue<SQLBIGINT>(type_info.auto_unique_value,
                                 auto_unique_value);
  ds_row.emplace_back(auto_unique_value);

  DSValue local_type_name;
  StringToDSValue(type_info.local_type_name, local_type_name);
  ds_row.emplace_back(local_type_name);

  DSValue minimum_scale;
  ArithmeticToDSValue<SQLBIGINT>(type_info.minimum_scale, minimum_scale);
  ds_row.emplace_back(minimum_scale);

  DSValue maximum_scale;
  ArithmeticToDSValue<SQLBIGINT>(type_info.maximum_scale, maximum_scale);
  ds_row.emplace_back(maximum_scale);

  DSValue sql_data_type;
  ArithmeticToDSValue<SQLBIGINT>(type_info.sql_data_type, sql_data_type);
  ds_row.emplace_back(sql_data_type);

  DSValue sql_datetime_sub;
  ArithmeticToDSValue<SQLBIGINT>(type_info.sql_datetime_sub, sql_datetime_sub);
  ds_row.emplace_back(sql_datetime_sub);

  DSValue num_prec_radix;
  ArithmeticToDSValue<SQLBIGINT>(type_info.num_prec_radix, num_prec_radix);
  ds_row.emplace_back(num_prec_radix);

  DSValue interval_precision;
  ArithmeticToDSValue<SQLBIGINT>(type_info.interval_precision,
                                 interval_precision);
  ds_row.emplace_back(interval_precision);

  return ds_row;
}

void CreateTypeInfoRowSchema(ResultSet& result_set) {
  RowSchema& row_schema = result_set.row_schema;

  // Schema for type_name
  row_schema.emplace_back(ColumnSchema{0, BQDataType::kString});

  // Schema for data_type
  row_schema.emplace_back(ColumnSchema{1, BQDataType::kInt64});

  // Schema for col_size
  row_schema.emplace_back(ColumnSchema{2, BQDataType::kInt64});

  // Schema for literal_prefix
  row_schema.emplace_back(ColumnSchema{3, BQDataType::kString});

  // Schema for literal_suffix
  row_schema.emplace_back(ColumnSchema{4, BQDataType::kString});

  // Schema for create_params
  row_schema.emplace_back(ColumnSchema{5, BQDataType::kString});

  // Schema for nullable
  row_schema.emplace_back(ColumnSchema{6, BQDataType::kInt64});

  // Schema for case_sensitive
  row_schema.emplace_back(ColumnSchema{7, BQDataType::kInt64});

  // Schema for searchable
  row_schema.emplace_back(ColumnSchema{8, BQDataType::kInt64});

  // Schema for unsigned_attribute
  row_schema.emplace_back(ColumnSchema{9, BQDataType::kInt64});

  // Schema for fixed_prec_scale
  row_schema.emplace_back(ColumnSchema{10, BQDataType::kInt64});

  // Schema for auto_unique_value
  row_schema.emplace_back(ColumnSchema{11, BQDataType::kInt64});

  // Schema for local_type_name
  row_schema.emplace_back(ColumnSchema{12, BQDataType::kString});

  // Schema for minimum_scale
  row_schema.emplace_back(ColumnSchema{13, BQDataType::kInt64});

  // Schema for maximum_scale
  row_schema.emplace_back(ColumnSchema{14, BQDataType::kInt64});

  // Schema for sql_data_type
  row_schema.emplace_back(ColumnSchema{15, BQDataType::kInt64});

  // Schema for sql_datetime_sub
  row_schema.emplace_back(ColumnSchema{16, BQDataType::kInt64});

  // Schema for num_prec_radix
  row_schema.emplace_back(ColumnSchema{17, BQDataType::kInt64});

  // Schema for interval_precision
  row_schema.emplace_back(ColumnSchema{18, BQDataType::kInt64});
}

void GetTypeInfoFromBQType(SQLSMALLINT const& sql_type,
                           std::string const& bq_type, bool const& is_array,
                           TypeInfoRow& type_info) {
  if (is_array) {
    type_info = kBqArrayTypeInfoRow;
  } else if (bq_type == "INTEGER") {
    type_info = kBqInt64TypeInfoRow;
  } else if (bq_type == "BOOLEAN") {
    type_info = kBqBoolTypeInfoRow;
  } else if (bq_type == "FLOAT") {
    type_info = kBqFloat64TypeInfoRow;
  } else if (bq_type == "STRUCT" || bq_type == "RECORD") {
    type_info = kBqStructTypeInfoRow;
  } else {
    if (kSqlToBqDataTypes.count(sql_type) > 0 &&
        kSqlToBqDataTypes.at(sql_type).count(bq_type) > 0) {
      type_info = kSqlToBqDataTypes.at(sql_type).at(bq_type);
    } else {
      type_info = kBqStringTypeInfoRow;
    }
  }
}

}  // namespace google::cloud::odbc_bq_driver_internal
