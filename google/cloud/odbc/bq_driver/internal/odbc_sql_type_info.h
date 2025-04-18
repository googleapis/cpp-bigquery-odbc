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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_TYPE_INFO_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_TYPE_INFO_H

#include "google/cloud/odbc/bq_driver/internal/odbc_query.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include <map>
#include <string>
#include <vector>

namespace google::cloud::odbc_bq_driver_internal {

struct TypeInfoRow {
  SQLCHAR* type_name;
  SQLSMALLINT data_type;
  SQLINTEGER col_size;
  SQLCHAR* literal_prefix;
  SQLCHAR* literal_suffix;
  SQLCHAR* create_params;
  SQLSMALLINT nullable;
  SQLSMALLINT case_sensitive;
  SQLSMALLINT searchable;
  SQLSMALLINT unsigned_attribute;
  SQLSMALLINT fixed_prec_scale;
  SQLSMALLINT auto_unique_value;
  SQLCHAR* local_type_name;
  SQLSMALLINT minimum_scale;
  SQLSMALLINT maximum_scale;
  SQLSMALLINT sql_data_type;
  SQLSMALLINT sql_datetime_sub;
  SQLINTEGER num_prec_radix;
  SQLSMALLINT interval_precision;
};

TypeInfoRow const kBqInt64TypeInfoRow = {
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("INT64")),  // type_name
    SQL_BIGINT,                                      // data_type
    19,                                              // col_size
    nullptr,                                         // literal_prefix
    nullptr,                                         // literal_suffix
    nullptr,                                         // create_params
    1,                                               // nullable
    0,                                               // case_sensitive
    2,                                               // searchable
    0,                                               // unsigned_attribute
    0,                                               // fixed_prec_scale
    NULL,                                            // auto_unique_value
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("INT64")),  // local_type_name
    0,                                               // minimum_scale
    0,                                               // maximum_scale
    SQL_BIGINT,                                      // sql_data_type
    NULL,                                            // sql_datetime_sub
    10,                                              // num_prec_radix
    NULL,                                            // interval_precision
};

TypeInfoRow const kBqBoolTypeInfoRow = {
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("BOOL")),  // type_name
    SQL_BIT,                                        // data_type
    1,                                              // col_size
    nullptr,                                        // literal_prefix
    nullptr,                                        // literal_suffix
    nullptr,                                        // create_params
    1,                                              // nullable
    0,                                              // case_sensitive
    2,                                              // searchable
    0,                                              // unsigned_attribute
    0,                                              // fixed_prec_scale
    NULL,                                           // auto_unique_value
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("BOOL")),  // local_type_name
    0,                                              // minimum_scale
    0,                                              // maximum_scale
    SQL_BIT,                                        // sql_data_type
    NULL,                                           // sql_datetime_sub
    10,                                             // num_prec_radix
    NULL,                                           // interval_precision
};

TypeInfoRow const kBqDateTypeInfoRow = {
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("DATE")),  // type_name
    SQL_TYPE_DATE,                                  // data_type
    10,                                             // col_size
    nullptr,                                        // literal_prefix
    nullptr,                                        // literal_suffix
    nullptr,                                        // create_params
    1,                                              // nullable
    0,                                              // case_sensitive
    2,                                              // searchable
    0,                                              // unsigned_attribute
    0,                                              // fixed_prec_scale
    NULL,                                           // auto_unique_value
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("DATE")),  // local_type_name
    0,                                              // minimum_scale
    0,                                              // maximum_scale
    SQL_DATETIME,                                   // sql_data_type
    1,                                              // sql_datetime_sub
    10,                                             // num_prec_radix
    NULL,                                           // interval_precision
};

TypeInfoRow const kBqFloat64TypeInfoRow = {
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("FLOAT64")),  // type_name
    SQL_DOUBLE,                                        // data_type
    53,                                                // col_size
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("\"")),  // literal_prefix
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("\"")),  // literal_suffix
    nullptr,                                      // create_params
    1,                                            // nullable
    0,                                            // case_sensitive
    2,                                            // searchable
    0,                                            // unsigned_attribute
    0,                                            // fixed_prec_scale
    NULL,                                         // auto_unique_value
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("FLOAT64")),  // local_type_name
    0,                                                 // minimum_scale
    0,                                                 // maximum_scale
    SQL_DOUBLE,                                        // sql_data_type
    1,                                                 // sql_datetime_sub
    2,                                                 // num_prec_radix
    NULL,                                              // interval_precision
};

TypeInfoRow const kBqTimeTypeInfoRow = {
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("TIME")),  // type_name
    SQL_TYPE_TIME,                                  // data_type
    15,                                             // col_size
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("\"")),  // literal_prefix
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("\"")),  // literal_suffix
    nullptr,                                      // create_params
    1,                                            // nullable
    0,                                            // case_sensitive
    3,                                            // searchable
    0,                                            // unsigned_attribute
    0,                                            // fixed_prec_scale
    NULL,                                         // auto_unique_value
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("TIME")),  // local_type_name
    0,                                              // minimum_scale
    0,                                              // maximum_scale
    SQL_DATETIME,                                   // sql_data_type
    2,                                              // sql_datetime_sub
    2,                                              // num_prec_radix
    NULL,                                           // interval_precision
};

TypeInfoRow const kBqTimestampTypeInfoRow = {
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("TIMESTAMP")),  // type_name
    SQL_TYPE_TIMESTAMP,                                  // data_type
    26,                                                  // col_size
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("\"")),  // literal_prefix
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("\"")),  // literal_suffix
    nullptr,                                      // create_params
    1,                                            // nullable
    0,                                            // case_sensitive
    3,                                            // searchable
    0,                                            // unsigned_attribute
    0,                                            // fixed_prec_scale
    NULL,                                         // auto_unique_value
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("TIMESTAMP")),  // local_type_name
    0,                                                   // minimum_scale
    6,                                                   // maximum_scale
    SQL_DATETIME,                                        // sql_data_type
    3,                                                   // sql_datetime_sub
    2,                                                   // num_prec_radix
    NULL,                                                // interval_precision
};

TypeInfoRow const kBqDatetimeTypeInfoRow = {
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("DATETIME")),  // type_name
    SQL_TYPE_TIMESTAMP,                                 // data_type
    26,                                                 // col_size
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("'")),  // literal_prefix
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("'")),  // literal_suffix
    nullptr,                                     // create_params
    1,                                           // nullable
    0,                                           // case_sensitive
    3,                                           // searchable
    0,                                           // unsigned_attribute
    0,                                           // fixed_prec_scale
    NULL,                                        // auto_unique_value
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("DATETIME")),  // local_type_name
    0,                                                  // minimum_scale
    0,                                                  // maximum_scale
    SQL_DATETIME,                                       // sql_data_type
    3,                                                  // sql_datetime_sub
    2,                                                  // num_prec_radix
    NULL,                                               // interval_precision
};

TypeInfoRow const kBqBytesTypeInfoRow = {
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("BYTES")),  // type_name
    SQL_VARBINARY,                                   // data_type
    16384,                                           // col_size
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("0x'")),  // literal_prefix
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("'")),  // literal_suffix
    nullptr,                                     // create_params
    1,                                           // nullable
    0,                                           // case_sensitive
    2,                                           // searchable
    0,                                           // unsigned_attribute
    0,                                           // fixed_prec_scale
    NULL,                                        // auto_unique_value
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("BYTES")),  // local_type_name
    0,                                               // minimum_scale
    0,                                               // maximum_scale
    SQL_VARBINARY,                                   // sql_data_type
    3,                                               // sql_datetime_sub
    2,                                               // num_prec_radix
    NULL,                                            // interval_precision
};

TypeInfoRow const kBqStringTypeInfoRow = {
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("STRING")),  // type_name
    SQL_VARCHAR,                                      // data_type
    16384,                                            // col_size
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("'")),  // literal_prefix
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("'")),  // literal_suffix
    nullptr,                                     // create_params
    1,                                           // nullable
    1,                                           // case_sensitive
    3,                                           // searchable
    0,                                           // unsigned_attribute
    0,                                           // fixed_prec_scale
    NULL,                                        // auto_unique_value
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("STRING")),  // local_type_name
    0,                                                // minimum_scale
    0,                                                // maximum_scale
    SQL_VARCHAR,                                      // sql_data_type
    3,                                                // sql_datetime_sub
    2,                                                // num_prec_radix
    NULL,                                             // interval_precision
};

TypeInfoRow const kBqArrayTypeInfoRow = {
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("ARRAY")),  // type_name
    SQL_VARCHAR,                                     // data_type
    16384,                                           // col_size
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("'")),  // literal_prefix
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("'")),  // literal_suffix
    nullptr,                                     // create_params
    0,                                           // nullable
    1,                                           // case_sensitive
    0,                                           // searchable
    0,                                           // unsigned_attribute
    0,                                           // fixed_prec_scale
    NULL,                                        // auto_unique_value
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("ARRAY")),  // local_type_name
    0,                                               // minimum_scale
    0,                                               // maximum_scale
    SQL_VARCHAR,                                     // sql_data_type
    3,                                               // sql_datetime_sub
    2,                                               // num_prec_radix
    NULL,                                            // interval_precision
};

TypeInfoRow const kBqStructTypeInfoRow = {
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("STRUCT")),  // type_name
    SQL_VARCHAR,                                      // data_type
    16384,                                            // col_size
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("'")),  // literal_prefix
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("'")),  // literal_suffix
    nullptr,                                     // create_params
    1,                                           // nullable
    1,                                           // case_sensitive
    0,                                           // searchable
    0,                                           // unsigned_attribute
    0,                                           // fixed_prec_scale
    NULL,                                        // auto_unique_value
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("STRUCT")),  // local_type_name
    0,                                                // minimum_scale
    0,                                                // maximum_scale
    SQL_VARCHAR,                                      // sql_data_type
    3,                                                // sql_datetime_sub
    2,                                                // num_prec_radix
    NULL,                                             // interval_precision
};

TypeInfoRow const kBqIntervalTypeInfoRow = {
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("INTERVAL")),  // type_name
    SQL_VARCHAR,                                        // data_type
    16384,                                              // col_size
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("'")),  // literal_prefix
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("'")),  // literal_suffix
    nullptr,                                     // create_params
    1,                                           // nullable
    0,                                           // case_sensitive
    3,                                           // searchable
    0,                                           // unsigned_attribute
    0,                                           // fixed_prec_scale
    NULL,                                        // auto_unique_value
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("INTERVAL")),  // local_type_name
    0,                                                  // minimum_scale
    0,                                                  // maximum_scale
    SQL_VARCHAR,                                        // sql_data_type
    3,                                                  // sql_datetime_sub
    2,                                                  // num_prec_radix
    NULL,                                               // interval_precision
};

TypeInfoRow const kBqJsonTypeInfoRow = {
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("JSON")),  // type_name
    SQL_VARCHAR,                                    // data_type
    16384,                                          // col_size
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("'")),  // literal_prefix
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("'")),  // literal_suffix
    nullptr,                                     // create_params
    1,                                           // nullable
    0,                                           // case_sensitive
    3,                                           // searchable
    0,                                           // unsigned_attribute
    0,                                           // fixed_prec_scale
    NULL,                                        // auto_unique_value
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("JSON")),  // local_type_name
    0,                                              // minimum_scale
    0,                                              // maximum_scale
    SQL_VARCHAR,                                    // sql_data_type
    3,                                              // sql_datetime_sub
    2,                                              // num_prec_radix
    NULL,                                           // interval_precision
};

TypeInfoRow const kBqGeographyTypeInfoRow = {
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("GEOGRAPHY")),  // type_name
    SQL_VARCHAR,                                         // data_type
    16384,                                               // col_size
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("'")),  // literal_prefix
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("'")),  // literal_suffix
    nullptr,                                     // create_params
    1,                                           // nullable
    1,                                           // case_sensitive
    0,                                           // searchable
    0,                                           // unsigned_attribute
    0,                                           // fixed_prec_scale
    NULL,                                        // auto_unique_value
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("GEOGRAPHY")),  // local_type_name
    0,                                                   // minimum_scale
    0,                                                   // maximum_scale
    SQL_VARCHAR,                                         // sql_data_type
    3,                                                   // sql_datetime_sub
    2,                                                   // num_prec_radix
    NULL,                                                // interval_precision
};

TypeInfoRow const kBqNumericTypeInfoRow = {
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("NUMERIC")),  // type_name
    SQL_NUMERIC,                                       // data_type
    38,                                                // col_size
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("'")),  // literal_prefix
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("'")),  // literal_suffix
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("PRECISION,SCALE")),  // create_params
    1,                                                         // nullable
    0,                                                         // case_sensitive
    2,                                                         // searchable
    0,     // unsigned_attribute
    1,     // fixed_prec_scale
    NULL,  // auto_unique_value
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("NUMERIC")),  // local_type_name
    9,                                                 // minimum_scale
    9,                                                 // maximum_scale
    SQL_NUMERIC,                                       // sql_data_type
    3,                                                 // sql_datetime_sub
    10,                                                // num_prec_radix
    NULL,                                              // interval_precision
};

TypeInfoRow const kBqBignumericTypeInfoRow = {
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("BIGNUMERIC")),  // type_name
    SQL_NUMERIC,                                          // data_type
    77,                                                   // col_size
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("'")),  // literal_prefix
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("'")),  // literal_suffix
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("PRECISION,SCALE")),  // create_params
    1,                                                         // nullable
    0,                                                         // case_sensitive
    2,                                                         // searchable
    0,     // unsigned_attribute
    1,     // fixed_prec_scale
    NULL,  // auto_unique_value
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("BIGNUMERIC")),  // local_type_name
    38,                                                   // minimum_scale,
    38,                                                   // maximum_scale
    SQL_NUMERIC,                                          // sql_data_type
    3,                                                    // sql_datetime_sub
    10,                                                   // num_prec_radix
    NULL,                                                 // interval_precision
};

std::map<SQLSMALLINT, std::map<std::string, TypeInfoRow>> const
    kSqlToBqDataTypes = {{SQL_BIGINT,
                          {
                              {"INT64", kBqInt64TypeInfoRow},
                          }},
                         {SQL_BIT,
                          {
                              {"BOOL", kBqBoolTypeInfoRow},
                          }},
                         {SQL_TYPE_DATE,
                          {
                              {"DATE", kBqDateTypeInfoRow},
                          }},
                         {SQL_DOUBLE,
                          {
                              {"FLOAT64", kBqFloat64TypeInfoRow},
                          }},
                         {SQL_TYPE_TIME,
                          {
                              {"TIME", kBqTimeTypeInfoRow},
                          }},
                         {SQL_TYPE_TIMESTAMP,
                          {
                              {"TIMESTAMP", kBqTimestampTypeInfoRow},
                              {"DATETIME", kBqDatetimeTypeInfoRow},
                          }},
                         {SQL_VARBINARY,
                          {
                              {"BYTES", kBqBytesTypeInfoRow},
                          }},
                         {SQL_VARCHAR,
                          {
                              {"STRING", kBqStringTypeInfoRow},
                              {"ARRAY", kBqArrayTypeInfoRow},
                              {"STRUCT", kBqStructTypeInfoRow},
                              {"INTERVAL", kBqIntervalTypeInfoRow},
                              {"JSON", kBqJsonTypeInfoRow},
                              {"GEOGRAPHY", kBqGeographyTypeInfoRow},
                          }},
                         {SQL_NUMERIC,
                          {
                              {"NUMERIC", kBqNumericTypeInfoRow},
                              {"BIGNUMERIC", kBqBignumericTypeInfoRow},
                          }}};

DSRow CreateDSRowFromTypeInfo(TypeInfoRow const& type_info);

void CreateTypeInfoRowSchema(ResultSet& result_set);

void GetTypeInfoFromBQType(SQLSMALLINT const& sql_type,
                           std::string const& bq_type, bool const& is_array,
                           TypeInfoRow& type_info);
class TypeInfoQuery : public Query {
 public:
  explicit TypeInfoQuery() = default;
  ~TypeInfoQuery() override = default;

  TypeInfoQuery(TypeInfoQuery const&) = default;
  TypeInfoQuery& operator=(TypeInfoQuery const&) = default;
  TypeInfoQuery(TypeInfoQuery&&) = default;
  TypeInfoQuery& operator=(TypeInfoQuery&&) = default;

 private:
  std::vector<TypeInfoRow> rows_;
};

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_TYPE_INFO_H
