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

#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/bq_client_interface/odbc_bq_client.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_conn_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_desc_attr.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include <regex>

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::Options;
using google::cloud::bigquery_v2_minimal_internal::Job;
using google::cloud::bigquery_v2_minimal_internal::JobStatistics;
using google::cloud::bigquery_v2_minimal_internal::TableSchema;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

DescriptorHandle& StatementHandle::GetDescriptorHandle(
    DescriptorType type) const {
  switch (type) {
    // DescriptorType::kApplication should not be used as input argument
    case DescriptorType::kApplication:
    case DescriptorType::kARD:
      return descriptors_.ard_expl_ != nullptr ? *descriptors_.ard_expl_
                                               : *descriptors_.ard_;
    case DescriptorType::kAPD:
      return descriptors_.apd_expl_ != nullptr ? *descriptors_.apd_expl_
                                               : *descriptors_.apd_;
    case DescriptorType::kIRD:
      return *descriptors_.ird_;
    case DescriptorType::kIPD:
      return *descriptors_.ipd_;
  }
}

void DissociateDescriptorHandle(DescriptorHandle* descriptor_handle,
                                DescriptorType type, StatementHandle* handle) {
  if (descriptor_handle) {
    descriptor_handle->GetAssociatedStatementHandles().erase({handle, type});
  }
}

void AssociateDescriptorHandle(DescriptorHandle* descriptor_handle,
                               DescriptorType type, StatementHandle* handle) {
  if (descriptor_handle) {
    descriptor_handle->GetAssociatedStatementHandles().emplace(handle, type);
  }
}

StatusRecord StatementHandle::SetDescriptorHandle(
    DescriptorType type, DescriptorHandle* descriptor_handle) {
  if (descriptor_handle &&
      descriptor_handle->GetHeaderRecord().GetAllocType() !=
          SQL_DESC_ALLOC_USER) {
    return StatusRecord{SQLStates::k_HY017(),
                        "Invalid setting of implicitly allocated descriptor"};
  }
  switch (type) {
    case DescriptorType::kARD:
      DissociateDescriptorHandle(descriptors_.ard_expl_, type, this);
      descriptors_.ard_expl_ = descriptor_handle;
      break;
    case DescriptorType::kAPD:
      DissociateDescriptorHandle(descriptors_.apd_expl_, type, this);
      descriptors_.apd_expl_ = descriptor_handle;
      break;
    default:
      return StatusRecord{SQLStates::k_HY017(),
                          "Invalid try to set implementation descriptor"};
  }
  AssociateDescriptorHandle(descriptor_handle, type, this);
  return StatusRecord::Ok();
}

StatusRecord StatementHandle::SetAttribute(int attribute, SQLULEN value) {
  StatusRecord status_record =
      ValidateStatementAttributeToSet(attribute, value);
  if (!status_record.ok()) {
    return status_record;
  }
  attributes_[attribute] = value;
  return StatusRecord::Ok();
}

StatusRecord StatementHandle::PopulateResultSet(TableSchema const& schema) {
  for (int i = 0; i < schema.fields.size(); ++i) {
    auto const& field = schema.fields[i];
    ColumnSchema column;

    column.col_index = i;

    StatusRecordOr<BQDataType> type_status_record = ConvertDSType(field.type);

    if (!type_status_record.Ok()) {
      return type_status_record.GetStatusRecord();
    }

    column.col_type = *type_status_record;
    result_set_.row_schema.emplace_back(column);
  }

  return StatusRecord::Ok();
}

StatusRecord StatementHandle::PrepareQuery(const SQLCHAR* query_text) {
  // TODO(b/342044533) Sanitize query text to avoid potential SQL Injection
  // risk.
  if (query_text == nullptr) {
    return StatusRecord{SQLStates::k_HY000(), "Query text is null"};
  }

  Job req;
  std::string query(reinterpret_cast<char const*>(query_text));
  req.configuration.query.query = query;
  req.configuration.query.use_query_cache = true;
  req.configuration.dry_run = true;
  req.configuration.query.use_legacy_sql = false;

  std::regex positional_pattern(R"(\?)");
  std::regex named_pattern(R"([:@]\w+)");

  // Check for positional parameters
  if (std::regex_search(query, positional_pattern)) {
    req.configuration.query.parameter_mode = "POSITIONAL";
  }

  // Check for named parameters
  if (std::regex_search(query, named_pattern)) {
    req.configuration.query.parameter_mode = "NAMED";
  }

  Options opt;

  auto response = this->GetConnectionHandle()->GetClient()->InsertJob(
      GetConnectionHandle()->GetDsn().catalog, req, opt);

  if (!response.Ok()) {
    return response.GetStatusRecord();
  }
  auto& schema = response.GetValue().statistics.job_query_stats.schema;

  auto pop_response = PopulateResultSet(schema);

  SetQueryParameters(
      response.GetValue()
          .statistics.job_query_stats.undeclared_query_parameters);

  if (!pop_response.ok()) {
    return pop_response;
  }

  query_str_ = query;
  stmt_state_ = StmtStates::kStatementPrepared;
  DescriptorHandle& desc_handle =
      this->GetDescriptorHandle(DescriptorType::kIPD);
  auto job_statistics = (*response).statistics;
  StatusRecord ipd_response = PopulateIpd(desc_handle, job_statistics);
  return StatusRecord::Ok();
}

StatusRecordOr<SQLULEN> StatementHandle::GetAttribute(int attribute) {
  if (!IsStatementAttributeValid(attribute)) {
    return StatusRecord{SQLStates::k_HY092(), "Invalid attribute"};
  }
  return attributes_[attribute];
}

StatusRecord StatementHandle::PopulateIpd(DescriptorHandle& handle,
                                          JobStatistics const& job_statistics) {
  DescriptorRecord descriptor_record;
  std::string const nullable = "NULLABLE";
  auto stmt_params = job_statistics.job_query_stats.undeclared_query_parameters;
  TableSchema schema = job_statistics.job_query_stats.schema;
  if (stmt_params.empty()) {
    return StatusRecord::Ok();
  }

  for (int i = 0; i < stmt_params.size(); i++) {
    StatusRecordOr<SQLSMALLINT> record_type =
        GetSQLDataType(stmt_params[i].parameter_type.type);

    descriptor_record.SetConciseType(*record_type, DescriptorType::kIPD);
    descriptor_record.SetName(stmt_params[i].name, stmt_params[i].name.size());
    descriptor_record.type_name = stmt_params[i].parameter_type.type;

    descriptor_record.nullable =
        schema.fields[i].mode == nullable ? SQL_NULLABLE : SQL_NO_NULLS;

    handle.BindNewDescriptorRecord(i + 1, descriptor_record);
  }

  return StatusRecord::Ok();
}

}  // namespace google::cloud::odbc_bq_driver_internal
