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
#include "google/cloud/odbc/bq_client_interface/setenv.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include "google/cloud/internal/getenv.h"
#include <gtest/gtest.h>
#include <regex>

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;
using google::cloud::odbc_testing_utils::StatusRecordIs;
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
  auto sections_status = ParseConfig(test_data_path + "/sample.ini");
  ASSERT_STATUS_RECORD_OK(sections_status);

  auto sections = *sections_status;

  // Test if the uncommented sections are defined
  for (auto const& it_outer : kSampleIniSections) {
    std::string section_name = it_outer.first;
    Section sample_ini_section = it_outer.second;
    for (auto& it_inner : sample_ini_section) {
      std::string property = it_inner.first;
      EXPECT_EQ(sample_ini_section[property],
                (*(sections))[section_name][property]);
    }
  }

  // Test if the commented sections are not defined
  for (auto const& it_outer : kCommentedIniSections) {
    std::string section_name = it_outer.first;
    Section commented_ini_section = it_outer.second;
    for (auto& it_inner : commented_ini_section) {
      std::string property = it_inner.first;
      EXPECT_EQ((*(sections))[section_name][property], "");
    }
  }
}

TEST(Parsing, ParseConfig_IncorrectPath) {
  auto sections = ParseConfig("/invalid_file_name.ini");
  EXPECT_THAT(sections, StatusRecordIs(SQLStates::k_HY000(),
                                       HasSubstr("Can't open file")));
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

  StatusRecordOr<Section> section_resp_status = ParseConnectionString(conn_str);
  ASSERT_STATUS_RECORD_OK(section_resp_status);

  auto section_resp = *section_resp_status;

  for (auto const& it : testing_section) {
    std::string field = it.first;
    std::string value = it.second;
    Trim(field);
    Trim(value);
    EXPECT_EQ(section_resp[field], value);
  }
}

TEST(Parsing, ParseConnectionString_InvalidString) {
  Section testing_section = kDsnSection;
  std::string conn_str = "a=3;b;";
  StatusRecordOr<Section> section_resp = ParseConnectionString(conn_str);
  EXPECT_THAT(section_resp,
              StatusRecordIs(SQLStates::k_HY000(), HasSubstr("Invalid")));
}

TEST(GetPathToOdbcIni, GetPath_EnvVar) {
  std::string expected = "my_path";
  google::cloud::odbc_bigquery_client_interface::SetEnv("ODBCINI", expected);

  std::string actual = GetPathToOdbcIni();

  EXPECT_EQ(actual, expected);
  google::cloud::odbc_bigquery_client_interface::UnsetEnv("ODBCINI");
}

TEST(GetPathToOdbcIni, GetPath_HomeVar) {
  ASSERT_TRUE(::google::cloud::internal::GetEnv("HOME"));

  std::string actual = GetPathToOdbcIni();

  EXPECT_THAT(actual, HasSubstr("/.odbc.ini"));
}

TEST(GetPathToOdbcIni, GetEmptyPath) {
  auto home = ::google::cloud::internal::GetEnv("HOME");
  google::cloud::odbc_bigquery_client_interface::UnsetEnv("HOME");

  std::string actual = GetPathToOdbcIni();

  EXPECT_EQ(actual, "");
  google::cloud::odbc_bigquery_client_interface::SetEnv("HOME", home);
}

TEST(FilterUsingOdbcRegex, UseBaseRegex) {
  auto regex = CastOdbcRegexToCppRegex("abcde");

  EXPECT_EQ("abcde", regex);
  std::regex odbc_regex(regex);
  EXPECT_TRUE(std::regex_match("abcde", odbc_regex));
  EXPECT_FALSE(std::regex_match("abcd", odbc_regex));
  EXPECT_FALSE(std::regex_match("abcd1", odbc_regex));
}

TEST(FilterUsingOdbcRegex, UsePercent) {
  auto regex = CastOdbcRegexToCppRegex("%abc%");

  EXPECT_EQ(".*abc.*", regex);
  std::regex odbc_regex(regex);
  EXPECT_TRUE(std::regex_match("abcde", odbc_regex));
  EXPECT_TRUE(std::regex_match("abc", odbc_regex));
  EXPECT_TRUE(std::regex_match("00abc", odbc_regex));
  EXPECT_FALSE(std::regex_match("ab1c", odbc_regex));
}

TEST(FilterUsingOdbcRegex, UsePercentWithEscapeCharacter) {
  auto regex = CastOdbcRegexToCppRegex("%a%b\\%c%");

  EXPECT_EQ(".*a.*b%c.*", regex);
  std::regex odbc_regex(regex);
  EXPECT_TRUE(std::regex_match("ab%cde", odbc_regex));
  EXPECT_TRUE(std::regex_match("ab%c", odbc_regex));
  EXPECT_FALSE(std::regex_match("00abc", odbc_regex));
  EXPECT_FALSE(std::regex_match("ab1c", odbc_regex));
}

TEST(FilterUsingOdbcRegex, UseUnderscore) {
  auto regex = CastOdbcRegexToCppRegex("_ab_c_");

  EXPECT_EQ(".ab.c.", regex);
  std::regex odbc_regex(regex);
  EXPECT_TRUE(std::regex_match("0ab1c2", odbc_regex));
  EXPECT_TRUE(std::regex_match("_ab_c_", odbc_regex));
  EXPECT_FALSE(std::regex_match("abc", odbc_regex));
  EXPECT_FALSE(std::regex_match("ab0c", odbc_regex));
}

TEST(FilterUsingOdbcRegex, UseUnderscoreWithEscapeCharacter) {
  auto regex = CastOdbcRegexToCppRegex("_ab\\_c_");

  EXPECT_EQ(".ab_c.", regex);
  std::regex odbc_regex(regex);
  EXPECT_TRUE(std::regex_match("0ab_c2", odbc_regex));
  EXPECT_TRUE(std::regex_match("_ab_c_", odbc_regex));
  EXPECT_FALSE(std::regex_match("0ab1c2", odbc_regex));
  EXPECT_FALSE(std::regex_match("ab_c", odbc_regex));
}

TEST(FilterUsingOdbcRegex, UseComplexPattern) {
  auto regex = CastOdbcRegexToCppRegex("\\%abc\\_def_ghi%");

  EXPECT_EQ("%abc_def.ghi.*", regex);
  std::regex odbc_regex(regex);
  EXPECT_TRUE(std::regex_match("%abc_def0ghi", odbc_regex));
  EXPECT_TRUE(std::regex_match("%abc_def0ghi11111", odbc_regex));
  EXPECT_TRUE(std::regex_match("%abc_def_ghi___", odbc_regex));
  EXPECT_FALSE(std::regex_match("abc_def0ghi", odbc_regex));
  EXPECT_FALSE(std::regex_match("%abc_defghi", odbc_regex));
  EXPECT_FALSE(std::regex_match("%abc_def00ghi", odbc_regex));
}

TEST(SplitTableTypes, SplitZeroTypes) {
  std::vector<std::string> types = SplitTableTypes("  ");

  EXPECT_EQ(1, types.size());
  EXPECT_EQ("", types[0]);
}

TEST(SplitTableTypes, SplitTwoTypesWithSpaces) {
  std::vector<std::string> types = SplitTableTypes(" TABLE , VIEW ");

  EXPECT_EQ(2, types.size());
  EXPECT_EQ("TABLE", types[0]);
  EXPECT_EQ("VIEW", types[1]);
}

TEST(SplitTableTypes, SplitTwoTypesWithQuotes) {
  std::vector<std::string> types = SplitTableTypes(" ' TABLE ' , ' VIEW ' ");

  EXPECT_EQ(2, types.size());
  EXPECT_EQ("TABLE", types[0]);
  EXPECT_EQ("VIEW", types[1]);
}

TEST(SplitTableTypes, SplitTwoTypesWithOneQuote) {
  std::vector<std::string> types = SplitTableTypes(" ' TABLE  ,  VIEW ' ");

  EXPECT_EQ(2, types.size());
  EXPECT_EQ("' TABLE", types[0]);
  EXPECT_EQ("VIEW '", types[1]);
}

TEST(UnicodeConversion, Success_UnicodeConversion) {
  std::wstring wstr = L"आपका स्वागत है";
  std::vector<wchar_t> sqlWStr(wstr.begin(), wstr.end());
  sqlWStr.emplace_back(L'\0');
  std::string result_str = Utf16ToUtf8(sqlWStr.data());
  EXPECT_EQ("आपका स्वागत है", result_str);
  std::wstring result_wstr = Utf8ToUtf16(result_str);
  EXPECT_STREQ(sqlWStr.data(), result_wstr.data());
}

}  // namespace google::cloud::odbc_bq_driver_internal
