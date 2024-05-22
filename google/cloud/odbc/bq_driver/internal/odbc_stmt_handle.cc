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

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::bigquery_v2_minimal_internal::QueryRequest;
using google::cloud::bigquery_v2_minimal_internal::TableSchema;
using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
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
      return descriptors_.ird_expl_ != nullptr ? *descriptors_.ird_expl_
                                               : *descriptors_.ird_;
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
    case DescriptorType::kIRD:
      DissociateDescriptorHandle(descriptors_.ird_expl_, type, this);
      descriptors_.ird_expl_ = descriptor_handle;
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

odbc_internal::StatusRecord StatementHandle::PopulatResultSet(
    TableSchema const& schema) {
  for (int i = 0; i < schema.fields.size(); ++i) {
    auto const& field = schema.fields[i];
    ColumnSchema column;

    column.col_index = i;

    StatusRecordOr<BQDataType> type_status_record = ConvertDSType(field.type);

    if (!type_status_record.Ok()) {
      return type_status_record.GetStatusRecord();
    }

    column.col_type = *type_status_record;
    result_set_.row_schema.push_back(column);
  }

  return StatusRecord::Ok();
}

StatusRecord StatementHandle::PrepareQuery(const SQLCHAR* query_text) {
  QueryRequest req;

  std::string query(reinterpret_cast<char const*>(query_text));

  req.set_query(query).set_dry_run(true);
  ::google::cloud::Options opt;

  auto response = this->GetConnectionHandle()->GetClient()->Query(
      GetConnectionHandle()->GetDsn().catalog, req, opt);

  if (response.Ok()) {
    auto& schema = response.GetValue().schema;
    auto* desc_handle =
        new DescriptorHandle(DescriptorType::kIRD, SQL_DESC_ALLOC_USER);
    StatusRecord status_record = PopulateIrd(desc_handle, schema);
    if (!status_record.ok()) {
      return StatusRecord{SQLStates::k_HY000(), "Internal error occurred"};
    }
    this->SetDescriptorHandle(DescriptorType::kIRD, desc_handle);
    query_str_ = query;
    return PopulatResultSet(schema);
  }

  return StatusRecord{SQLStates::k_HY000(), "Internal error occurred"};
}

odbc_internal::StatusRecord StatementHandle::PopulateIrd(
    DescriptorHandle* descriptor_handle, TableSchema const& schema) {
  std::map<SQLSMALLINT, DescriptorRecord> records;
  for (auto const& res : schema.fields) {
    DescriptorRecord descriptor_record;
    descriptor_record.SetName(res.name, SQL_NTS);
    descriptor_record.length = res.max_length;
    descriptor_record.precision = res.precision;
    StatusRecordOr<BQDataType> type_status_record = ConvertDSType(res.type);

    if (!type_status_record.Ok()) {
      return type_status_record.GetStatusRecord();
    }

    StatusRecord status_record = descriptor_record.SetConciseType(
        *type_status_record, DescriptorType::kIRD);
    if (!status_record.ok()) {
      return status_record;
    }
    descriptor_record.nullable =
        res.mode == "NULLABLE" ? SQL_NULLABLE : SQL_NO_NULLS;
    records[records.size() + 1] = descriptor_record;
  }

  StatusRecord status_record = descriptor_handle->SetDescriptorRecords(records);

  if (!status_record.ok()) {
    descriptor_handle->GetDiagnostics().AddStatusRecord(status_record);
  }

  return status_record;
}

StatusRecordOr<SQLULEN> StatementHandle::GetAttribute(int attribute) {
  if (!IsStatementAttributeValid(attribute)) {
    return StatusRecord{SQLStates::k_HY092(), "Invalid attribute"};
  }
  return attributes_[attribute];
}

}  // namespace google::cloud::odbc_bq_driver_internal
