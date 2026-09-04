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


#include "google/cloud/odbc/bq_driver/internal/odbc_sql_statistics.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::SQLStates;

StatusRecord ValidateStatisticsParameters(
    const SQLCHAR* catalog_name, SQLSMALLINT catalog_name_len,
    const SQLCHAR* schema_name, SQLSMALLINT schema_name_len,
    const SQLCHAR* table_name, SQLSMALLINT table_name_len,
    SQLUSMALLINT index_type, SQLUSMALLINT reserved,
    SQLULEN metadata_id) {
 // Validate table and table related parameters. 
  auto status_record = ValidateTableParameters(
      catalog_name, catalog_name_len, schema_name, schema_name_len, table_name,
      table_name_len, metadata_id);
  if (!status_record.ok()) {
    LOG(ERROR) << "ValidateStatisticsParameters::ValidateTableParameters:: "
               << status_record.message;
    return status_record;
  }

  if(index_type != SQL_INDEX_UNIQUE && index_type != SQL_INDEX_ALL){
    return StatusRecord{SQLStates::k_HY100(),"Invalid index_type - index type is invalid"};
  }

  if(reserved !=SQL_QUICK && reserved != SQL_ENSURE){
    return StatusRecord{SQLStates::k_HY100(),"Invalid reserved value - reserved value is invalid"};

  }
  return StatusRecord::Ok();
}


}  // namespace google::cloud::odbc_bq_driver_internal
