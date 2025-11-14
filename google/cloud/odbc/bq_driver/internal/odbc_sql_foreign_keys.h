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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_FOREIGN_KEYS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_FOREIGN_KEYS_H

#include "google/cloud/odbc/bq_client_interface/odbc_bq_client.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_conn_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_execute_utils.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include <map>
#include <string>
#include <vector>

namespace google::cloud::odbc_bq_driver_internal {

// Executes a BQ query and fetches the foreign key results and
// populates the DSResults, as mentioned below:
//
// 1) First makes a call to ODBCBQClient::Query()
// 2) If Query() finishes within the timeout and returns all results then no
//    further action is needed and the function returns. In this case,
//    the PostQueryResults will be populated in DSResults structure.
// 3) If Query() does not finish in specified timeout then a subsequent call is
// made to
//    ODBCBQClient::GetAllQueryResults() to fetch all the results. In this case,
//    the GetQueryResults will be populated in DSResults structure.
//

static const std::map<int, OdbcColumnSpec> kODBCForeignKeysMap = {
    {0, OdbcColumnSpec{"PKTABLE_CAT", SQL_WVARCHAR, 128, 0, SQL_TRUE}},     
    {1, OdbcColumnSpec{"PKTABLE_SCHEM", SQL_WVARCHAR, 1024, 0, SQL_TRUE}},  
    {2, OdbcColumnSpec{"PKTABLE_NAME", SQL_WVARCHAR, 1024, 0, SQL_FALSE}},  
    {3, OdbcColumnSpec{"PKCOLUMN_NAME", SQL_WVARCHAR, 128, 0, SQL_FALSE}},  
    {4, OdbcColumnSpec{"FKTABLE_CAT", SQL_WVARCHAR, 128, 0, SQL_TRUE}},     
    {5, OdbcColumnSpec{"FKTABLE_SCHEM", SQL_WVARCHAR, 1024, 0, SQL_TRUE}},  
    {6, OdbcColumnSpec{"FKTABLE_NAME", SQL_WVARCHAR, 1024, 0, SQL_FALSE}},  
    {7, OdbcColumnSpec{"FKCOLUMN_NAME", SQL_WVARCHAR, 128, 0, SQL_FALSE}},  
    {8, OdbcColumnSpec{"KEY_SEQ", SQL_SMALLINT, 5, 0, SQL_FALSE}},         
    {9, OdbcColumnSpec{"UPDATE_RULE", SQL_SMALLINT, 5, 0, SQL_TRUE}},    
    {10, OdbcColumnSpec{"DELETE_RULE", SQL_SMALLINT, 5, 0, SQL_TRUE}},    
    {11, OdbcColumnSpec{"FK_NAME", SQL_WVARCHAR, 128, 0, SQL_TRUE}},       
    {12, OdbcColumnSpec{"PK_NAME", SQL_WVARCHAR, 128, 0, SQL_TRUE}},       
    {13, OdbcColumnSpec{"DEFERRABILITY", SQL_SMALLINT, 5, 0, SQL_TRUE}},   
};

odbc_internal::StatusRecordOr<DSResults> FetchForeignKeysFromDataSource(
    StatementHandle& stmt_handle, std::string const& pk_catalog_name,
    int pk_catalog_name_len, std::string const& pk_schema_name,
    int pk_schema_name_len, std::string const& pk_table_name,
    int pk_table_name_len, std::string const& fk_catalog_name,
    int fk_catalog_name_len, std::string const& fk_schema_name,
    int fk_schema_name_len, std::string const& fk_table_name,
    int fk_table_name_len);

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_FOREIGN_KEYS_H
