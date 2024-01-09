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

#include "google/cloud/odbc/bq_driver/internal/utils.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include "google/cloud/internal/getenv.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_testing_utils::StatusIs;
using ::testing::HasSubstr;

Section const kDsnSection{
    {"Description", "Google BigQuery ODBC Connector"},
    {"Driver",
     "/opt/odbc-driver/googlebigqueryodbc/lib/libgooglebigqueryodbc_sb64.so"},
    {"PropertyWithoutValue", ""},
    {"Value with equals", "I=am=a=value"}};

Section const kOdbcSection{{"Trace", "1"}, {"TraceFile", "/tmp/odbc.log"}};

Section const kCommentedDsnSection{{"HTAPI_MinResultsSize", "1000"},
                                   {"HTAPI_MinActivationRatio", "3"}};

Sections const kSampleIniSections{{"SampleDSN", kDsnSection},
                                  {"ODBC", kOdbcSection}};

Sections const kCommentedIniSections{
    {"SampleDSN", kCommentedDsnSection},
};

TEST(StringUtils, Split_Basic) {
  std::string s = "SOFTWARE\\ODBC\\ODBC.INI";
  std::vector<std::string> v = Split(s, "\\", 2);
  std::vector<std::string> v_expected{"SOFTWARE", "ODBC\\ODBC.INI"};
  ASSERT_EQ(v_expected.size(), v.size());
  for (int i = 0; i < v_expected.size(); i++) {
    ASSERT_EQ(v_expected[i], v[i]);
  }
}

TEST(StringUtils, Split_DefaultParams) {
  std::string s = "SOFTWARE ODBC ODBC.INI ";
  std::vector<std::string> v = Split(s);
  std::vector<std::string> v_expected{"SOFTWARE", "ODBC", "ODBC.INI", ""};
  ASSERT_EQ(v_expected.size(), v.size());
  for (int i = 0; i < v_expected.size(); i++) {
    ASSERT_EQ(v_expected[i], v[i]);
  }
}

TEST(StringUtils, NoSplitPossible) {
  std::string s = "SOFTWARE\\ODBC\\ODBC.INI";
  std::vector<std::string> v = Split(s, "random_delimiter");
  ASSERT_EQ(v.size(), 1);
  EXPECT_EQ(v[0], s);
}

TEST(StringUtils, Join_Basic) {
  std::vector<std::string> v{"SOFTWARE", "ODBC", "ODBC.INI"};
  std::string s_expected_1 = "ODBC\\ODBC.INI";
  std::string s = Join(v, "\\", 1);
  EXPECT_EQ(s_expected_1, s);

  std::string s_expected_0 = "SOFTWARE\\ODBC\\ODBC.INI";
  s = Join(v, "\\", 0);
  EXPECT_EQ(s_expected_0, s);
}

TEST(StringUtils, Join_DefaultParams) {
  std::vector<std::string> v{"SOFTWARE", "ODBC", "ODBC.INI"};
  std::string s_expected = "SOFTWAREODBCODBC.INI";
  std::string s = Join(v);
  EXPECT_EQ(s_expected, s);
}

TEST(StringUtils, Join_StartIndOutOfRange) {
  std::vector<std::string> v{"SOFTWARE", "ODBC", "ODBC.INI"};
  std::string s_expected = "";
  std::string s = Join(v, "_", 3);
  EXPECT_EQ(s_expected, s);
}

TEST(Parsing, ParseConfig) {
  std::string test_data_path =
      google::cloud::internal::GetEnv("CPP_BIGQUERY_ODBC_DRIVER_TEST_DATA_PATH")
          .value_or("");
  auto sections = ParseConfig(test_data_path + "/sample.ini");

  // Test if the uncommented sections are defined
  for (auto const& it_outer : kSampleIniSections) {
    std::string section_name = it_outer.first;
    Section sample_ini_section = it_outer.second;
    for (auto& it_inner : sample_ini_section) {
      std::string property = it_inner.first;
      EXPECT_EQ(sample_ini_section[property],
                (*(sections.value()))[section_name][property]);
    }
  }

  // Test if the commented sections are not defined
  for (auto const& it_outer : kCommentedIniSections) {
    std::string section_name = it_outer.first;
    Section commented_ini_section = it_outer.second;
    for (auto& it_inner : commented_ini_section) {
      std::string property = it_inner.first;
      EXPECT_EQ((*(sections.value()))[section_name][property], "");
    }
  }
}

TEST(Parsing, ParseConfig_IncorrectPath) {
  auto sections = ParseConfig("/invalid_file_name.ini");
  EXPECT_EQ(sections.status().code(), StatusCode::kInvalidArgument);
}

TEST(Parsing, ParseConnectionString) {
  Section testing_section = kDsnSection;
  std::string conn_str = "";
  // Create the connection string we will use for this test
  for (auto const& it : testing_section) {
    conn_str.append(it.first);
    conn_str.append("=");
    conn_str.append(it.second);
    conn_str.append(";");
  }

  StatusOr<Section> section_resp = ParseConnectionString(conn_str);
  ASSERT_STATUS_OK(section_resp);

  for (auto const& it : testing_section) {
    std::string field = it.first;
    std::string value = it.second;
    Trim(field);
    Trim(value);
    EXPECT_EQ(section_resp.value()[field], value);
  }
}

TEST(Parsing, ParseConnectionString_InvalidString) {
  Section testing_section = kDsnSection;
  std::string conn_str = "a=3;b;";
  StatusOr<Section> section_resp = ParseConnectionString(conn_str);
  EXPECT_THAT(section_resp,
              StatusIs(StatusCode::kInvalidArgument, HasSubstr("Invalid")));
}

}  // namespace google::cloud::odbc_bq_driver_internal
