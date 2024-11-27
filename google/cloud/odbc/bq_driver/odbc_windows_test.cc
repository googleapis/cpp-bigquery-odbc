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
  HWND hwndParent = NULL;
  WORD fRequest = ODBC_ADD_DSN;
  LPCSTR lpszDriver = NULL;
  LPCSTR lpszAttributes =
      "DSN=Personnel Data\0UID=Smith\0PWD=Sesame\0DATABASE=Personnel\0\0";
  auto status =
      ConfigDSNInternal(hwndParent, fRequest, lpszDriver, lpszAttributes);
  EXPECT_EQ(status, false);
}

TEST(ConfigDSNInternal, NullAttributes) {
  HWND hwndParent = NULL;
  WORD fRequest = ODBC_ADD_DSN;
  LPCSTR lpszDriver = NULL;
  LPCSTR lpszAttributes = NULL;
  auto status =
      ConfigDSNInternal(hwndParent, fRequest, lpszDriver, lpszAttributes);
  EXPECT_EQ(status, false);
}

TEST(ConfigDSNInternal, NullRequest) {
  HWND hwndParent = NULL;
  WORD fRequest = NULL;
  LPCSTR lpszDriver = "SQL Server";
  LPCSTR lpszAttributes =
      "DSN=Personnel Data\0UID=Smith\0PWD=Sesame\0DATABASE=Personnel\0\0";
  auto status =
      ConfigDSNInternal(hwndParent, fRequest, lpszDriver, lpszAttributes);
  EXPECT_EQ(status, false);
}

TEST(ConfigDSNInternal, NullhandleSuccess) {
  HWND hwndParent = NULL;
  WORD fRequest = ODBC_ADD_DSN;
  LPCSTR lpszDriver = "ODBC Driver For Google Bigquery";
  LPCSTR lpszAttributes =
      "DSN=Personnel Data\0Email=Smith.Sesame@gmail.com\0Dataset=Personnel\0\0";
  auto result =
      ConfigDSNInternal(hwndParent, fRequest, lpszDriver, lpszAttributes);
  EXPECT_EQ(result, true);
  auto status = GetSectionWin("SOFTWARE\\ODBC\\ODBC.INI\\Personnel Data");
  std::shared_ptr<Section> section2 = status.GetValue();
  ASSERT_TRUE(section2);

  EXPECT_EQ(section2->at("Email"), "Smith.Sesame@gmail.com");
  EXPECT_EQ(section2->at("Dataset"), "Personnel");
  result = ConfigDSNInternal(hwndParent, ODBC_REMOVE_DSN, lpszDriver,
                             lpszAttributes);
  EXPECT_EQ(result, true);
}

}  // namespace google::cloud::odbc_bq_driver