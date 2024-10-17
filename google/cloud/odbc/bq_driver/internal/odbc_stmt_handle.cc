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
#include "google/cloud/odbc/bq_driver/internal/odbc_desc_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_type_info.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_transactions.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include <regex>

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::Options;
using google::cloud::bigquery_v2_minimal_internal::Job;
using google::cloud::bigquery_v2_minimal_internal::JobStatistics;
using google::cloud::bigquery_v2_minimal_internal::TableSchema;
using google::cloud::odbc_bq_driver_internal::BeginTransactionIfNeeded;
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

StatementHandle ::StatementHandle(StatementHandle const& statementHandle)
    : Handle(statementHandle) {
  kType = statementHandle.kType;
  stmt_state_ = statementHandle.stmt_state_;
  result_set_ = statementHandle.result_set_;
  query_str_ = statementHandle.query_str_;
  descriptors_ = statementHandle.descriptors_;
  query_ = statementHandle.query_;
  attributes_ = statementHandle.attributes_;
  // TODO(b/349757194): Convert shallow copy to deep copy
  conn_handle_ = statementHandle.conn_handle_;
  query_parameters_ = statementHandle.query_parameters_;
};

StatementHandle& StatementHandle::operator=(
    StatementHandle const& statementHandle) {
  if (this != &statementHandle) {
    kType = statementHandle.kType;
    stmt_state_ = statementHandle.stmt_state_;
    result_set_ = statementHandle.result_set_;
    query_str_ = statementHandle.query_str_;
    descriptors_ = statementHandle.descriptors_;
    query_ = statementHandle.query_;
    attributes_ = statementHandle.attributes_;
    // TODO(b/349757194): Convert shallow copy to deep copy
    conn_handle_ = statementHandle.conn_handle_;
    query_parameters_ = statementHandle.query_parameters_;
  }
  return *this;
}

StatementHandle::StatementHandle(StatementHandle&& statementHandle) noexcept {
  kType = std::move(statementHandle.kType);
  stmt_state_ = std::move(statementHandle.stmt_state_);
  result_set_ = std::move(statementHandle.result_set_);
  query_str_ = std::move(statementHandle.query_str_);
  descriptors_ = std::move(statementHandle.descriptors_);
  query_ = std::move(statementHandle.query_);
  attributes_ = std::move(statementHandle.attributes_);
  conn_handle_ = std::move(statementHandle.conn_handle_);
  query_parameters_ = std::move(statementHandle.query_parameters_);
}
StatementHandle& StatementHandle::operator=(
    StatementHandle&& statementHandle) noexcept {
  kType = std::move(statementHandle.kType);
  stmt_state_ = std::move(statementHandle.stmt_state_);
  result_set_ = std::move(statementHandle.result_set_);
  query_str_ = std::move(statementHandle.query_str_);
  descriptors_ = std::move(statementHandle.descriptors_);
  query_ = std::move(statementHandle.query_);
  attributes_ = std::move(statementHandle.attributes_);
  conn_handle_ = std::move(statementHandle.conn_handle_);
  query_parameters_ = std::move(statementHandle.query_parameters_);
  return *this;
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

  StatusRecord transaction_status = BeginTransactionIfNeeded(*conn_handle_);
  if (!transaction_status.ok()) {
    return transaction_status;
  }

  Job req;
  std::string query(reinterpret_cast<char const*>(query_text));
  req.configuration.query.query = query;
  req.configuration.query.use_query_cache = true;
  req.configuration.dry_run = true;
  req.configuration.query.use_legacy_sql = false;

  // Add default dataset from the config
  ConnectionHandle& conn_handle = *GetConnectionHandle();
  // Not defining a catalog or default dataset is an internal error
  // and indicates connection handle was not initialized correctly
  // and we cannot proceed with the server request without it.
  std::string catalog_name = conn_handle.GetDsn().catalog;
  if (catalog_name.empty()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Internal error: catalog cannot be empty"};
  }
  std::string default_dataset = conn_handle.GetDsn().default_dataset;
  if (!default_dataset.empty()) {
    req.configuration.query.default_dataset.project_id = catalog_name;
    req.configuration.query.default_dataset.dataset_id = default_dataset;
  } else {
    return StatusRecord{SQLStates::k_HY000(),
                        "Internal error: default dataset cannot be empty"};
  }

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
  if (conn_handle.IsSessionStarted()) {
    req.configuration.query.connection_properties.push_back(
        {"session_id", conn_handle.GetSessionId()});
  } else if (conn_handle.GetDsn().sessions_enabled) {
    req.configuration.query.create_session = true;
  }

  Options opt;

  auto response = conn_handle.GetClient()->InsertJob(
      conn_handle.GetDsn().catalog, req, opt);

  if (!response.Ok()) {
    return response.GetStatusRecord();
  }

  auto& schema = response.GetValue().statistics.job_query_stats.schema;
  auto pop_response = PopulateResultSet(schema);
  if (!pop_response.ok()) {
    return pop_response;
  }

  SetQueryParameters(
      response.GetValue()
          .statistics.job_query_stats.undeclared_query_parameters);

  if (!pop_response.ok()) {
    return pop_response;
  }

  DescriptorHandle& desc_handle =
      this->GetDescriptorHandle(DescriptorType::kIRD);
  desc_handle.ClearDescriptorRecordsMap();
  StatusRecord ird_response = PopulateIrd(desc_handle, schema);
  if (!ird_response.ok()) {
    return ird_response;
  }

  DescriptorHandle& ipd_desc_handle =
      this->GetDescriptorHandle(DescriptorType::kIPD);
  ipd_desc_handle.ClearDescriptorRecordsMap();
  auto job_statistics = (*response).statistics;
  StatusRecord ipd_response = PopulateIpd(ipd_desc_handle, job_statistics);
  if (!ipd_response.ok()) {
    return ipd_response;
  }
  if (!conn_handle.IsSessionStarted() &&
      !response->statistics.session_info.session_id.empty()) {
    conn_handle.SetSessionId(response->statistics.session_info.session_id);
  }

  query_str_ = query;
  prepared_job_ = *response;
  stmt_state_ = StmtStates::kStatementPrepared;

  return StatusRecord::Ok();
}

StatusRecord StatementHandle::PopulateIrd(DescriptorHandle& descriptor_handle,
                                          TableSchema const& schema) {
  if (&descriptor_handle == nullptr ||
      descriptor_handle.GetType() != DescriptorType::kIRD) {
    return StatusRecord{SQLStates::k_HY024(),
                        "Invalid attribute value (invalid descriptor handle)"};
  }
  std::string const nullable = "NULLABLE";
  std::string const nullable_required = "REQUIRED";
  std::string const array_field = "REPEATED";
  for (int i = 0; i < schema.fields.size(); ++i) {
    auto const& res = schema.fields[i];
    DescriptorRecord descriptor_record;
    descriptor_record.SetName(res.name, res.name.length());
    descriptor_record.length = res.max_length;
    StatusRecordOr<SQLSMALLINT> type_status_record =
        GetSQLDataType(res.type, (res.mode == array_field));

    if (!type_status_record.Ok()) {
      return type_status_record.GetStatusRecord();
    }
    StatusRecord status_record = descriptor_record.SetConciseType(
        *type_status_record, DescriptorType::kIRD);
    if (!status_record.ok()) {
      return status_record;
    }

    TypeInfoRow type_info;
    GetTypeInfoFromBQType(type_status_record.GetValue(), res.type,
                          res.mode == array_field, type_info);

    if (res.type == "TIME" || res.type == "DATETIME") {
      descriptor_record.precision = 6;
      descriptor_record.scale = 6;
    } else if (res.type == "TIMESTAMP" || res.type == "DATE") {
      descriptor_record.precision;
      descriptor_record.scale = type_info.maximum_scale;
    } else {
      descriptor_record.precision = type_info.interval_precision == NULL
                                        ? type_info.col_size
                                        : type_info.interval_precision;
      descriptor_record.scale = type_info.maximum_scale;
    }
    if (type_status_record.GetValue() == SQL_DOUBLE) {
      // hard-coding to 15 to have the same behaviour as internal driver
      descriptor_record.length = 15;
    } else {
      descriptor_record.length = type_info.col_size;
    }

    descriptor_record.nullable = (res.mode == nullable) ? SQL_NULLABLE
                                 : (res.mode == nullable_required)
                                     ? SQL_NULLABLE
                                     : SQL_NO_NULLS;

    descriptor_handle.BindNewDescriptorRecord(i + 1, descriptor_record);
  }
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
  if (handle.GetType() != DescriptorType::kIPD) {
    return StatusRecord(
        {SQLStates::k_HY024(),
         "Invalid attribute value (invalid descriptor handle)"});
  }
  DescriptorRecord descriptor_record;
  std::string const nullable = "NULLABLE";
  std::string const nullable_required = "REQUIRED";
  std::string const array_field = "REPEATED";
  auto stmt_params = job_statistics.job_query_stats.undeclared_query_parameters;
  TableSchema schema = job_statistics.job_query_stats.schema;
  if (stmt_params.empty()) {
    return StatusRecord::Ok();
  }

  for (int i = 0; i < stmt_params.size(); i++) {
    StatusRecordOr<SQLSMALLINT> record_type =
        GetSQLDataType(stmt_params[i].parameter_type.type,
                       (schema.fields[i].mode == array_field));
    descriptor_record.SetConciseType(*record_type, DescriptorType::kIPD);
    descriptor_record.SetName(stmt_params[i].name, stmt_params[i].name.size());
    descriptor_record.type_name = stmt_params[i].parameter_type.type;

    descriptor_record.nullable =
        (schema.fields[i].mode == nullable)            ? SQL_NULLABLE
        : (schema.fields[i].mode == nullable_required) ? SQL_NULLABLE
                                                       : SQL_NO_NULLS;

    TypeInfoRow type_info;
    GetTypeInfoFromBQType(record_type.GetValue(),
                          stmt_params[i].parameter_type.type,
                          schema.fields[i].mode == array_field, type_info);

    if (stmt_params[i].parameter_type.type == "TIME" ||
        stmt_params[i].parameter_type.type == "DATETIME") {
      descriptor_record.precision = 6;
      descriptor_record.scale = 6;
    } else if (stmt_params[i].parameter_type.type == "TIMESTAMP" ||
               stmt_params[i].parameter_type.type == "DATE") {
      descriptor_record.precision;
    } else {
      descriptor_record.precision = type_info.interval_precision == NULL
                                        ? type_info.col_size
                                        : type_info.interval_precision;
      descriptor_record.scale = type_info.maximum_scale;
    }

    if (record_type.GetValue() == SQL_DOUBLE) {
      // hard-coding to 15 to have the same behaviour as internal driver
      descriptor_record.length = 15;
    } else {
      descriptor_record.length = type_info.col_size;
    }

    handle.BindNewDescriptorRecord(i + 1, descriptor_record);
  }

  return StatusRecord::Ok();
}

void StatementHandle::CloseCursor() {
  ResultSet result_set;
  result_set_ = result_set;
  if (WasStatementPrepared()) {
    SetStmtState(StmtStates::kStatementPrepared);
  } else {
    SetStmtState(StmtStates::kStatementNotPrepared);
  }
}

}  // namespace google::cloud::odbc_bq_driver_internal
