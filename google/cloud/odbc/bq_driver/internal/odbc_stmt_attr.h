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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_STMT_ATTR_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_STMT_ATTR_H

#include "google/cloud/odbc/bq_driver/internal/odbc_desc_handle.h"
#include "google/cloud/odbc/internal/diagnostic_records.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include <map>
#include <memory>

namespace google::cloud::odbc_bq_driver_internal {

static std::map<int, SQLULEN> const kDefaultAttributes = {
    {SQL_ATTR_ASYNC_ENABLE, SQL_ASYNC_ENABLE_OFF},
    {SQL_ATTR_CONCURRENCY, SQL_CONCUR_READ_ONLY},
    {SQL_ATTR_CURSOR_SCROLLABLE, SQL_NONSCROLLABLE},
    {SQL_ATTR_CURSOR_SENSITIVITY, SQL_UNSPECIFIED},
    {SQL_ATTR_CURSOR_TYPE, SQL_CURSOR_FORWARD_ONLY},
    {SQL_ATTR_ENABLE_AUTO_IPD, SQL_TRUE},
    {SQL_ATTR_MAX_LENGTH, 0},
    {SQL_ATTR_MAX_ROWS, 0},
    {SQL_ATTR_METADATA_ID, SQL_FALSE},
    {SQL_ATTR_NOSCAN, SQL_NOSCAN_OFF},
    {SQL_ATTR_QUERY_TIMEOUT, 0},
    {SQL_ATTR_RETRIEVE_DATA, SQL_RD_ON},
    {SQL_ATTR_ROW_NUMBER, 0},
    {SQL_ATTR_USE_BOOKMARKS, SQL_UB_OFF},
};

struct Descriptors {
  Descriptors() = default;
  Descriptors(DescriptorHandle const& ard, DescriptorHandle const& apd,
              DescriptorHandle const& ird, DescriptorHandle const& ipd)
      : ard_(std::make_unique<DescriptorHandle>(ard)),
        apd_(std::make_unique<DescriptorHandle>(apd)),
        ird_(std::make_unique<DescriptorHandle>(ird)),
        ipd_(std::make_unique<DescriptorHandle>(ipd)){};

  Descriptors(Descriptors const& descriptors);

  Descriptors& operator=(Descriptors const& descriptors);

  std::unique_ptr<DescriptorHandle> ard_;
  DescriptorHandle* ard_expl_ = nullptr;
  std::unique_ptr<DescriptorHandle> apd_;
  DescriptorHandle* apd_expl_ = nullptr;
  std::unique_ptr<DescriptorHandle> ird_;
  std::unique_ptr<DescriptorHandle> ipd_;
};

bool IsStatementAttributeValid(int attribute);

odbc_internal::StatusRecord ValidateStatementAttributeToSet(int attribute,
                                                            SQLULEN value);

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_STMT_ATTR_H
