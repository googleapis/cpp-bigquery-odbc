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

#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_attr.h"
#include "google/cloud/odbc/internal/diagnostic_records.h"

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;

static std::map<int, std::vector<SQLULEN>> const kAttrPossibleValues = {
    {SQL_ATTR_ASYNC_ENABLE, {SQL_ASYNC_ENABLE_OFF, SQL_ASYNC_ENABLE_ON}},
    {SQL_ATTR_CONCURRENCY, {SQL_CONCUR_READ_ONLY}},
    {SQL_ATTR_CURSOR_SCROLLABLE, {SQL_NONSCROLLABLE}},
    {SQL_ATTR_CURSOR_SENSITIVITY, {SQL_INSENSITIVE}},
    {SQL_ATTR_CURSOR_TYPE, {SQL_CURSOR_FORWARD_ONLY}},
    {SQL_ATTR_ENABLE_AUTO_IPD, {SQL_TRUE, SQL_FALSE}},
    {SQL_ATTR_MAX_LENGTH, {}},
    {SQL_ATTR_MAX_ROWS, {}},
    {SQL_ATTR_METADATA_ID, {SQL_TRUE, SQL_FALSE}},
    {SQL_ATTR_NOSCAN, {SQL_NOSCAN_OFF, SQL_NOSCAN_ON}},
    {SQL_ATTR_QUERY_TIMEOUT, {}},
    {SQL_ATTR_RETRIEVE_DATA, {SQL_RD_ON, SQL_RD_OFF}},
    {SQL_ATTR_USE_BOOKMARKS, {SQL_UB_OFF}},
};

bool IsStatementAttributeValid(int attribute) {
  return kAttrPossibleValues.count(attribute) != 0;
}

bool IsValueValidForStatementAttribute(int attribute, SQLULEN value) {
  std::vector<SQLULEN> possible_values = kAttrPossibleValues.at(attribute);
  return possible_values.empty() ||
         (std::find(possible_values.begin(), possible_values.end(), value) !=
          possible_values.end());
}

Descriptors::Descriptors(Descriptors const& descriptors) {
  ard_ = std::make_unique<DescriptorHandle>(*descriptors.ard_);
  apd_ = std::make_unique<DescriptorHandle>(*descriptors.apd_);
  ird_ = std::make_unique<DescriptorHandle>(*descriptors.ird_);
  ipd_ = std::make_unique<DescriptorHandle>(*descriptors.ipd_);
};

Descriptors& Descriptors::operator=(Descriptors const& descriptors) {
  if (this != &descriptors) {
    ard_ = std::make_unique<DescriptorHandle>(*descriptors.ard_);
    apd_ = std::make_unique<DescriptorHandle>(*descriptors.apd_);
    ird_ = std::make_unique<DescriptorHandle>(*descriptors.ird_);
    ipd_ = std::make_unique<DescriptorHandle>(*descriptors.ipd_);
  }
  return *this;
};

StatusRecord ValidateStatementAttributeToSet(int attribute, SQLULEN value) {
  if (!IsStatementAttributeValid(attribute)) {
    LOG(ERROR) << "ValidateStatementAttributeToSet:: Invalid attribute: "
               << attribute;
    return StatusRecord{SQLStates::k_HY092(), "Invalid attribute"};
  }
  if (!IsValueValidForStatementAttribute(attribute, value)) {
    LOG(ERROR) << "ValidateStatementAttributeToSet:: Invalid value: " << value
               << " for attribute: " << attribute;
    return StatusRecord{SQLStates::k_HY024(), "Invalid attribute value"};
  }
  return StatusRecord::Ok();
}

}  // namespace google::cloud::odbc_bq_driver_internal
