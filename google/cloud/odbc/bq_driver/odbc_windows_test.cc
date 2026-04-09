// Copyright 2023 Google LLC
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

#include "google/cloud/odbc/bq_driver/odbc_windows.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver {
using google::cloud::odbc_bq_driver_internal::GetSectionWin;
using google::cloud::odbc_bq_driver_internal::Section;

TEST(ConfigDSNInternal, NullDriverDetails) {
  HWND hwnd_parent = NULL;
  WORD f_request = ODBC_ADD_DSN;
  LPCSTR lpsz_driver = NULL;
  LPCSTR lpsz_attributes =
      "DSN=Personnel Data\0UID=Smith\0PWD=Sesame\0DATABASE=Personnel\0\0";
  auto status =
      ConfigDSNInternal(hwnd_parent, f_request, lpsz_driver, lpsz_attributes);
  EXPECT_EQ(status, false);
}

TEST(ConfigDSNInternal, NullAttributes) {
  HWND hwnd_parent = NULL;
  WORD f_request = ODBC_ADD_DSN;
  LPCSTR lpsz_driver = NULL;
  LPCSTR lpsz_attributes = NULL;
  auto status =
      ConfigDSNInternal(hwnd_parent, f_request, lpsz_driver, lpsz_attributes);
  EXPECT_EQ(status, false);
}

TEST(ConfigDSNInternal, NullRequest) {
  HWND hwnd_parent = NULL;
  WORD f_request = NULL;
  LPCSTR lpsz_driver = "SQL Server";
  LPCSTR lpsz_attributes =
      "DSN=Personnel Data\0UID=Smith\0PWD=Sesame\0DATABASE=Personnel\0\0";
  auto status =
      ConfigDSNInternal(hwnd_parent, f_request, lpsz_driver, lpsz_attributes);
  EXPECT_EQ(status, false);
}

TEST(ConfigDSNInternal, NullhandleSuccess) {
  HWND hwnd_parent = NULL;
  WORD f_request = ODBC_ADD_DSN;
  LPCSTR lpsz_driver = "ODBC Driver For BigQuery";
  LPCSTR lpsz_attributes =
      "DSN=Personnel Data\0Email=Smith.Sesame@gmail.com\0Dataset=Personnel\0\0";
  auto result =
      ConfigDSNInternal(hwnd_parent, f_request, lpsz_driver, lpsz_attributes);
  EXPECT_EQ(result, true);
  auto status = GetSectionWin("SOFTWARE\\ODBC\\ODBC.INI\\Personnel Data");
  std::shared_ptr<Section> section2 = status.GetValue();
  ASSERT_TRUE(section2);

  EXPECT_EQ(section2->at("Email"), "Smith.Sesame@gmail.com");
  EXPECT_EQ(section2->at("DefaultDataset"), "Personnel");
  EXPECT_EQ(section2->at("MaxThreads"), "8");
  EXPECT_EQ(section2->at("MaxRetries"), "6");
  EXPECT_EQ(section2->at("LargeResultsDatasetId"), "_odbc_temp_tables");
  EXPECT_EQ(section2->at("LargeResultsTempTableExpirationTime"), "3600000");
  EXPECT_EQ(section2->at("RowsFetchedPerBlock"), "100000");
  EXPECT_EQ(section2->at("DefaultStringColumnLength"), "16384");
  result = ConfigDSNInternal(hwnd_parent, ODBC_REMOVE_DSN, lpsz_driver,
                             lpsz_attributes);
  EXPECT_EQ(result, true);
}

TEST(ConvertLogLevel, ValidateLogLevelConversion) {
  // Success
  EXPECT_EQ(ConvertLogLevel("LOG_INFO"), "3");
  EXPECT_EQ(ConvertLogLevel("LOG_OFF"), "0");

  // Invalid
  EXPECT_EQ(ConvertLogLevel("Invalid"), "");
  EXPECT_EQ(ConvertLogLevel(""), "");
  EXPECT_EQ(ConvertLogLevel("LOG"), "");
}
}  // namespace google::cloud::odbc_bq_driver
