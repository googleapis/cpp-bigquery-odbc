// Copyright 2026 Google LLC
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

#include "google/cloud/odbc/bq_driver/internal/odbc_sql_special_columns.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_columns.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_columns_utils.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_primary_keys.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::odbc_internal::StatusRecordOr;

StatusRecordOr<ResultSet> FetchSpecialColumnsResultSetFromTableMetaData(
    StatementHandle& /*stmt_handle*/, SQLUSMALLINT /*identifier_type*/,
    std::string const& /*catalog_name*/, int /*catalog_name_len*/,
    std::string const& /*schema_name*/, int /*schema_name_len*/,
    std::string const& /*table_name*/, int /*table_name_len*/,
    SQLUSMALLINT /*min_row_id_scope*/, SQLUSMALLINT /*col_nullable*/) {
  LOG(INFO) << "FetchSpecialColumnsResultSetFromTableMetaData:: Start";
  ResultSet result_set;

  for (auto const& [_, schema] : kSpecialColumnsMap) {
    result_set.row_schema.emplace_back(schema);
  }

  // BigQuery does not support ROWID/OID pseudo-columns or auto-updated row
  // versions, so SQL_ROWVER legitimately returns an empty result set.
  // Furthermore, since Primary Keys in BigQuery are NOT ENFORCED and do not
  // guarantee uniqueness, advertising them as SQL_BEST_ROWID would be dangerous
  // (an app could attempt a positioned update that hits multiple rows). The
  // ODBC spec explicitly allows returning an empty result set in these cases.
  // We do the same as the existing driver.
  return result_set;
}

}  // namespace google::cloud::odbc_bq_driver_internal
