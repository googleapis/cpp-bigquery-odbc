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

// TODO(b/342044533) Sanitize query text to avoid potential SQL Injection
// risk.
StatusRecord StatementHandle::PrepareQuery(std::string const& query) {
  StatusRecord transaction_status = BeginTransactionIfNeeded(*conn_handle_);
  if (!transaction_status.ok()) {
    return transaction_status;
  }
  ConnectionHandle& conn_handle = *GetConnectionHandle();

  Job req;
  req.configuration.query.query = query;
  req.configuration.query.use_query_cache = conn_handle.GetDsn().is_query_cache;
  req.configuration.dry_run = true;
  req.configuration.query.use_legacy_sql =
      conn_handle.GetDsn().is_bq_legacy_sql;

  // Add default dataset from the config
  std::string catalog_name = conn_handle.GetDsn().catalog;
  std::string default_dataset = conn_handle.GetDsn().default_dataset;
  if (!default_dataset.empty()) {
    req.configuration.query.default_dataset.project_id = catalog_name;
    req.configuration.query.default_dataset.dataset_id = default_dataset;
  }

  if (!conn_handle.GetDsn().is_bq_legacy_sql) {
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
  }

  std::vector<ConnectionProperty> combined_properties =
      conn_handle.GetDsn().connection_properties;

  // If session started, add session_id
  if (conn_handle.IsSessionStarted()) {
    combined_properties.push_back({"session_id", conn_handle.GetSessionId()});
  } else if (conn_handle.GetDsn().sessions_enabled) {
    req.configuration.query.create_session = true;
  }

  req.configuration.query.connection_properties = combined_properties;

  Options opt;
std::cout<<"SQLMoreResults kanchan:: reuest job details "<<req.DebugString("here ")<<std::endl;
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

  TableReference table_fields;
  auto table_ref =
      response.GetValue().statistics.job_query_stats.referenced_tables;
  if (!table_ref.empty()) {
    auto* table_ref_ptr = table_ref.data();
    table_fields = *table_ref_ptr;
  } else {
    table_fields = response.GetValue().configuration.query.destination_table;
  }

  DescriptorHandle& desc_handle =
      this->GetDescriptorHandle(DescriptorType::kIRD);
  desc_handle.ClearDescriptorRecordsMap();
  StatusRecord ird_response = PopulateIrd(desc_handle, schema, table_fields);
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
  return StatusRecord::Ok();
}

StatusRecord StatementHandle::PopulateIrd(DescriptorHandle& descriptor_handle,
                                          TableSchema const& schema,
                                          TableReference const& table_fields) {
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

    descriptor_record.base_column_name = res.name;
    descriptor_record.base_table_name = table_fields.table_id;
    descriptor_record.catalog_name = table_fields.project_id;
    descriptor_record.schema_name = table_fields.dataset_id;
    descriptor_record.table_name = table_fields.table_id;
    descriptor_record.label = res.name;

    descriptor_record.local_type_name =
        type_info.local_type_name
            ? std::string(reinterpret_cast<char*>(type_info.local_type_name))
            : "";
    descriptor_record.type_name =
        type_info.type_name
            ? std::string(reinterpret_cast<char*>(type_info.type_name))
            : "";

    descriptor_record.case_sensitive = type_info.case_sensitive;
    if (descriptor_record.local_type_name == "BIGNUMERIC" ||
        descriptor_record.local_type_name == "INT64" ||
        descriptor_record.local_type_name == "NUMERIC") {
      descriptor_record.SetNumPrecRadix(kNumPrecRadixForExactNumeric);
      descriptor_record.sql_desc_unsigned =
          (type_info.unsigned_attribute) ? SQL_TRUE : SQL_FALSE;
    } else if (descriptor_record.local_type_name == "FLOAT64") {
      descriptor_record.sql_desc_unsigned =
          (type_info.unsigned_attribute) ? SQL_TRUE : SQL_FALSE;
      descriptor_record.SetNumPrecRadix(kNumPrecRadixForApproximateNumeric);
    } else {
      descriptor_record.sql_desc_unsigned =
          (type_info.unsigned_attribute) ? SQL_FALSE : SQL_TRUE;
      descriptor_record.SetNumPrecRadix(kDefaultIntervalPrecision);
    }

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
    if (res.type == "DATETIME") {
      // hard-coding to 16 to have the same behaviour as existing driver
      descriptor_record.octet_length = 16;
    } else {
      descriptor_record.SetOctetLength(type_status_record.GetValue(),
                                       type_info.col_size,
                                       descriptor_record.precision);
    }
    if (res.type == "INTERVAL" || res.type == "JSON") {
      descriptor_record.case_sensitive = 1;
    }

    if (type_status_record.GetValue() == SQL_DOUBLE) {
      // hard-coding to 15 to have the same behaviour as existing driver
      descriptor_record.length = 15;
    } else {
      descriptor_record.length = type_info.col_size;
    }

    descriptor_record.nullable = (res.mode == nullable) ? SQL_NULLABLE
                                 : (res.mode == nullable_required)
                                     ? SQL_NULLABLE
                                     : SQL_NO_NULLS;
    // hard-coding to have the same behaviour as existing driver
    if (descriptor_record.local_type_name == "BIGNUMERIC" ||
        descriptor_record.local_type_name == "FLOAT64" ||
        descriptor_record.local_type_name == "NUMERIC") {
      descriptor_record.literal_prefix = "";
      descriptor_record.literal_suffix = "";
    } else if (descriptor_record.local_type_name == "BYTES") {
      descriptor_record.literal_prefix = "0x";
      descriptor_record.literal_suffix = "";
    } else if (descriptor_record.local_type_name == "DATE" ||
               descriptor_record.local_type_name == "TIME" ||
               descriptor_record.local_type_name == "TIMESTAMP") {
      descriptor_record.literal_prefix = "'";
      descriptor_record.literal_suffix = "'";
    } else {
      descriptor_record.literal_prefix =
          type_info.literal_prefix == nullptr
              ? ""
              : std::string(reinterpret_cast<char*>(type_info.literal_prefix));

      descriptor_record.literal_suffix =
          type_info.literal_suffix == nullptr
              ? ""
              : std::string(reinterpret_cast<char*>(type_info.literal_suffix));
    }
    descriptor_record.SetDisplaySize(type_status_record.GetValue(),
                                     type_info.col_size,
                                     descriptor_record.precision);
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
  auto stmt_params = job_statistics.job_query_stats.undeclared_query_parameters;
  if (stmt_params.empty()) {
    return StatusRecord::Ok();
  }

  for (int i = 0; i < stmt_params.size(); i++) {
    bool is_array = stmt_params[i].parameter_type.type == "ARRAY";
    StatusRecordOr<SQLSMALLINT> record_type =
        GetSQLDataType(stmt_params[i].parameter_type.type, is_array);
    descriptor_record.SetConciseType(*record_type, DescriptorType::kIPD);
    descriptor_record.SetName(stmt_params[i].name, stmt_params[i].name.size());
    descriptor_record.type_name = stmt_params[i].parameter_type.type;

    TypeInfoRow type_info;
    GetTypeInfoFromBQType(record_type.GetValue(),
                          stmt_params[i].parameter_type.type, is_array,
                          type_info);

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
  if (StatementPrepared()) {
    SetStmtState(StmtStates::kStatementPrepared);
  } else {
    SetStmtState(StmtStates::kStatementNotPrepared);
  }
}

}  // namespace google::cloud::odbc_bq_driver_internal
