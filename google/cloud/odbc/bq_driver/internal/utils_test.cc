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
using google::cloud::odbc_testing_utils::StatusRecIs;
using google::cloud::odbc_testing_utils::StatusRecordIs;
using ::testing::HasSubstr;

#ifdef _WIN32
Section const kDsnSection{{"Description", "ODBC Driver for Google BigQuery 1"},
                          {"Driver", "Simba ODBC Driver for Google BigQuery"},

                          {"SQLDialect", "1"},
                          {"AllowLargeResults", "0"},
                          {"Catalog", "bigquery-devtools-drivers"},
                          {"LargeResultsTempTableExpirationTime", "3600000"},
                          {"OAuthMechanism", "0"}};

Sections const kSampleIniSections{
    {"SampleDSN", kDsnSection},
};

#else
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

#endif  // _WIN32

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
#ifdef _WIN32

#ifdef _WIN64
  auto sections_status = ParseConfig("SOFTWARE\\ODBC\\ODBC.INI");
#else
  auto sections_status = ParseConfig("SOFTWARE\\WOW6432Node\\ODBC\\ODBC.INI");
#endif  // _WIN64

#else
  std::string test_data_path =
      google::cloud::internal::GetEnv("CPP_BIGQUERY_ODBC_DRIVER_TEST_DATA_PATH")
          .value_or("");
  auto sections_status = ParseConfig(test_data_path + "/sample.ini");
#endif  // _WIN32

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
#ifndef _WIN32
  for (auto const& it_outer : kCommentedIniSections) {
    std::string section_name = it_outer.first;
    Section commented_ini_section = it_outer.second;
    for (auto& it_inner : commented_ini_section) {
      std::string property = it_inner.first;
      EXPECT_EQ((*(sections))[section_name][property], "");
    }
  }
#endif
}

TEST(Parsing, ParseConfig_IncorrectPath) {
#ifdef _WIN32
#ifdef _WIN64
  auto sections = ParseConfig("SOFTWARE\\ODBC\\ODBC1.INI");
#else
  auto sections = ParseConfig("SOFTWARE\\WOW6432Node\\ODBC\\ODBC1.INI");
#endif
  EXPECT_THAT(sections,
              StatusRecordIs(SQLStates::k_HY000(),
                             HasSubstr("Can't open registry key with path")));
#else
  auto sections = ParseConfig("/invalid_file_name.ini");
  EXPECT_THAT(sections, StatusRecordIs(SQLStates::k_HY000(),
                                       HasSubstr("Can't open file")));
#endif  // _WIN32
}

#ifndef _WIN32
TEST(GetPathToOdbcIni, GetPath_EnvVar) {
  std::string expected = "my_path";
  google::cloud::odbc_bigquery_client_interface::SetEnv("ODBCINI", expected);

  std::string actual = GetPathToOdbcIni();

  EXPECT_EQ(actual, expected);
  google::cloud::odbc_bigquery_client_interface::UnsetEnv("ODBCINI");
}
#endif

TEST(GetPathToOdbcIni, GetPath_HomeVar) {
#ifdef _WIN32
  ASSERT_TRUE(::google::cloud::internal::GetEnv("ODBC_TESTS_DSN"));
#else
  ASSERT_TRUE(::google::cloud::internal::GetEnv("HOME"));
#endif  // _WIN32
  std::string actual = GetPathToOdbcIni();
#ifdef _WIN32
#ifdef _WIN64
  EXPECT_THAT(actual, HasSubstr("SOFTWARE\\ODBC\\"));
#else
  EXPECT_THAT(actual, HasSubstr("WOW6432Node\\ODBC"));
#endif
#else
  EXPECT_THAT(actual, HasSubstr("/.odbc.ini"));
#endif  // _WIN32
}

#ifndef _WIN32
TEST(GetPathToOdbcIni, GetEmptyPath) {
  auto home = ::google::cloud::internal::GetEnv("HOME");
  google::cloud::odbc_bigquery_client_interface::UnsetEnv("HOME");

  std::string actual = GetPathToOdbcIni();

  EXPECT_EQ(actual, "");
  google::cloud::odbc_bigquery_client_interface::SetEnv("HOME", home);
}
#endif  // _WIN32

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

TEST(Parsing, ParseConnectionString_DuplicateFields) {
  Section testing_section = kDsnSection;
  std::string conn_str = "a=3;a=4;";
  StatusRecordOr<Section> section_resp_status = ParseConnectionString(conn_str);
  ASSERT_STATUS_RECORD_OK(section_resp_status);
  Section section_resp = *section_resp_status;
  EXPECT_EQ(section_resp["a"], "3");
}

TEST(Parsing, ParseConnectionString_RemoveCurlyBraces) {
  std::string conn_str = "a={3};b=4;";
  StatusRecordOr<Section> section_resp_status = ParseConnectionString(conn_str);
  ASSERT_STATUS_RECORD_OK(section_resp_status);
  Section section_resp = *section_resp_status;

  EXPECT_EQ(section_resp["a"], "3");
  EXPECT_EQ(section_resp["b"], "4");

  EXPECT_NE(section_resp["a"], "{3}");
  EXPECT_NE(section_resp["a"], "3}");
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

TEST(UnicodeConversion, Success_ConvertSQLWCHARToString) {
  std::wstring query(
      L"INSERT INTO INTEGRATION_TESTS.Test_Table VALUES(4, 'अच्छा', 28)");
  std::vector<SQLWCHAR> sqlWStr(query.begin(), query.end());
  sqlWStr.emplace_back(L'\0');

  SQLWCHAR* statementText = sqlWStr.data();

  SQLSMALLINT length = sqlWStr.size();

  auto result_str = ConvertSQLWCHARToString(statementText, length);

  EXPECT_STREQ("INSERT INTO INTEGRATION_TESTS.Test_Table VALUES(4, 'अच्छा', 28)",
               result_str->c_str());
  auto result_wstr = Utf8ToUtf16(*result_str);
  EXPECT_STREQ(query.data(), result_wstr->data());
}

TEST(UnicodeConversion, Success_Utf16ToUtf8) {
  std::wstring wstr = L"आपका स्वागत है";
  std::vector<wchar_t> sqlWStr(wstr.begin(), wstr.end());
  sqlWStr.emplace_back(L'\0');
  auto result_str = Utf16ToUtf8(sqlWStr.data());
  ASSERT_FALSE(result_str->empty());
  auto result_wstr = Utf8ToUtf16(*result_str);
  EXPECT_STREQ(sqlWStr.data(), result_wstr->data());
}

TEST(UnicodeConversion, Success_Utf16ToUtf8_chinese) {
  std::wstring wstr = L"你好，先生，你好吗";
  std::vector<wchar_t> sqlWStr(wstr.begin(), wstr.end());
  sqlWStr.emplace_back(L'\0');
  auto result_str = Utf16ToUtf8(sqlWStr.data());
  ASSERT_FALSE(result_str->empty());
  auto result_wstr = Utf8ToUtf16(*result_str);
  EXPECT_STREQ(sqlWStr.data(), result_wstr->data());
}

TEST(UnicodeConversion, EmptyData_Utf16ToUtf8) {
  std::wstring wstr = L"";
  std::vector<wchar_t> sqlWStr(wstr.begin(), wstr.end());
  sqlWStr.emplace_back(L'\0');
  auto result_str = Utf16ToUtf8(sqlWStr.data());
  EXPECT_THAT(result_str, StatusRecordIs(SQLStates::k_HY000(),
                                         HasSubstr("string is empty/Null")));
}

TEST(DiagIdentifierString, IsDiagIdentifierString_true) {
  EXPECT_TRUE(IsDiagIdentifierString(SQL_DIAG_DYNAMIC_FUNCTION));
  EXPECT_TRUE(IsDiagIdentifierString(SQL_DIAG_CONNECTION_NAME));
  EXPECT_TRUE(IsDiagIdentifierString(SQL_DIAG_SERVER_NAME));
}

TEST(DiagIdentifierString, IsDiagIdentifierString_false) {
  EXPECT_FALSE(IsDiagIdentifierString(SQL_DIAG_DYNAMIC_FUNCTION_CODE));
}

TEST(IsFieldIdentifierString, IsFieldIdentifierString_true) {
  EXPECT_TRUE(IsFieldIdentifierString(SQL_DESC_BASE_COLUMN_NAME));
  EXPECT_TRUE(IsFieldIdentifierString(SQL_DESC_BASE_TABLE_NAME));
  EXPECT_TRUE(IsFieldIdentifierString(SQL_DESC_CATALOG_NAME));
}

TEST(IsFieldIdentifierString, IsFieldIdentifierString_false) {
  EXPECT_FALSE(IsFieldIdentifierString(SQL_DESC_MAXIMUM_SCALE));
}

TEST(IsInfoTypeString, IsInfoTypeString_true) {
  EXPECT_TRUE(IsInfoTypeString(SQL_CATALOG_NAME));
  EXPECT_TRUE(IsInfoTypeString(SQL_CATALOG_NAME_SEPARATOR));
  EXPECT_TRUE(IsInfoTypeString(SQL_COLLATION_SEQ));
}

TEST(IsInfoTypeString, IsInfoTypeString_false) {
  EXPECT_FALSE(IsInfoTypeString(SQL_INDEX_KEYWORDS));
}

TEST(CheckTargetType, CheckTargetType_true) {
  EXPECT_TRUE(CheckTargetType(SQL_C_CHAR));
  EXPECT_TRUE(CheckTargetType(SQL_C_FLOAT));
  EXPECT_TRUE(CheckTargetType(SQL_C_TYPE_DATE));
}

TEST(CheckTargetType, CheckTargetType_false) {
  EXPECT_FALSE(CheckTargetType(SQL_C_DATE));
}

TEST(IsSearchPatternArgument, SearchPattern_Percent) {
  EXPECT_TRUE(IsSearchPatternArgument("%"));
}

TEST(IsSearchPatternArgument, SearchPattern_Underscore) {
  EXPECT_TRUE(IsSearchPatternArgument("_"));
}

TEST(IsSearchPatternArgument, SearchPattern_Escape) {
  EXPECT_TRUE(IsSearchPatternArgument("\\"));
}

TEST(IsSearchPatternArgument, OrdinaryArgument) {
  EXPECT_FALSE(IsSearchPatternArgument("ordinary"));
}

TEST(IsQuotedIDArgument, SingleQuote) {
  EXPECT_TRUE(IsQuotedIDArgument("'arg'"));
}

TEST(IsQuotedIDArgument, DoubleQuotes) {
  EXPECT_TRUE(IsQuotedIDArgument("\"arg\""));
}

TEST(IsQuotedIDArgument, NoQuotes) { EXPECT_FALSE(IsQuotedIDArgument("arg")); }

TEST(RemoveQuotes, SingleQuote) {
  std::string in("'test'");
  RemoveQuotes(in);
  EXPECT_EQ(in, "test");
}

TEST(RemoveQuotes, DoubleQuotes) {
  std::string in("\"test\"");
  RemoveQuotes(in);
  EXPECT_EQ(in, "test");
}

TEST(RemoveQuotes, NoQuotes) {
  std::string in("test");
  RemoveQuotes(in);
  EXPECT_EQ(in, "test");

  RemoveQuotes(in);
  EXPECT_EQ(in, "test");
}

TEST(SanitizeIdentifierArgument, QuotedArgument_SingleQuote) {
  std::string arg("      'test'     ");
  SanitizeIdentifierArgument(arg);
  EXPECT_EQ(arg, "test");
}

TEST(SanitizeIdentifierArgument, QuotedArgument_DoubleQuotes) {
  std::string arg("      \"test\"     ");
  SanitizeIdentifierArgument(arg);
  EXPECT_EQ(arg, "test");
}

TEST(SanitizeIdentifierArgument, ArgumentWithoutQuotes) {
  std::string arg(" test     ");
  SanitizeIdentifierArgument(arg);
  EXPECT_EQ(arg, " TEST");
}

TEST(PopulateOutputConnectionString, Success) {
  SQLCHAR out_conn_str[50] = {0};
  SQLSMALLINT out_conn_str_len;
  std::string conn_string = "DSN=SampleDSN";

  auto result = PopulateOutputConnectionString(
      out_conn_str, sizeof(out_conn_str), &out_conn_str_len, conn_string);

  EXPECT_EQ(result.ok(), true);
  EXPECT_STREQ(reinterpret_cast<char*>(out_conn_str), "DSN=SampleDSN;");
  EXPECT_EQ(out_conn_str_len, strlen("DSN=SampleDSN;"));
}

TEST(PopulateOutputConnectionString, Fail_Truncated) {
  SQLCHAR out_conn_str[10] = {0};
  SQLSMALLINT out_conn_str_len;
  std::string conn_string = "DSN=SampleDSN";

  auto result = PopulateOutputConnectionString(
      out_conn_str, sizeof(out_conn_str), &out_conn_str_len, conn_string);

  EXPECT_EQ(result.sql_state, SQLStates::k_01004());
  EXPECT_EQ(result.message, "String data, right truncated");

  EXPECT_STREQ(reinterpret_cast<char*>(out_conn_str), "DSN=Sampl");
  EXPECT_NE(out_conn_str_len, conn_string.size());
}

TEST(PopulateOutputConnectionString, EmptyConnectionString) {
  SQLCHAR out_conn_str[10] = {0};
  SQLSMALLINT out_conn_str_len;
  std::string conn_string = "";

  auto result = PopulateOutputConnectionString(
      out_conn_str, sizeof(out_conn_str), &out_conn_str_len, conn_string);

  EXPECT_EQ(result.ok(), false);
  EXPECT_EQ(result.sql_state, SQLStates::k_HY000());
  EXPECT_EQ(result.message, "Invalid Connection String");
}

#ifdef _WIN32
std::string kTestDsn = "TestDSN";
std::string kDriver = "TestDriver";
std::string kEmail = "test@example.com";
std::string kOAuthMechanism = "0";
std::string kKeyFilePath = "C:\\path\\to\\keyfile";
std::string kCatalog = "TestCatalog";
std::string kDataset = "TestDataset";

Section CreateTestSection() {
  Section section;
  section["Email"] = kEmail;
  section["KeyFilePath"] = kKeyFilePath;
  section["OAuthMechanism"] = kOAuthMechanism;
  section["Catalog"] = kCatalog;
  section["Dataset"] = kDataset;
  return section;
}

TEST(AddDSNToRegistry, successfulAssertions) {
  Section section = CreateTestSection();
  StatusRecord result = AddDSNToRegistry(kTestDsn, kDriver, section);
  ASSERT_TRUE(result.ok());

  auto status = GetSectionWin("SOFTWARE\\ODBC\\ODBC.INI\\" + kTestDsn);
  std::shared_ptr<Section> section2 = status.GetValue();
  ASSERT_TRUE(section2);

  EXPECT_EQ(section2->at("Email"), kEmail);
  EXPECT_EQ(section2->at("KeyFilePath"), kKeyFilePath);
  EXPECT_EQ(section2->at("OAuthMechanism"), "0");
  EXPECT_EQ(section2->at("Catalog"), kCatalog);
  EXPECT_EQ(section2->at("Dataset"), kDataset);

  result = RemoveDSNFromRegistry(kTestDsn);
  ASSERT_TRUE(result.ok());
}

TEST(EditDSNInRegistry, successEdit) {
  Section section = CreateTestSection();
  StatusRecord result = AddDSNToRegistry(kTestDsn, kDriver, section);
  ASSERT_TRUE(result.ok());

  section["Email"] = "test@gmail.com";
  section["KeyFilePath"] = "C:\\path\\to\\abc";
  result = EditDSNInRegistry(kTestDsn, section);
  ASSERT_TRUE(result.ok());
  auto status = GetSectionWin("SOFTWARE\\ODBC\\ODBC.INI\\" + kTestDsn);
  std::shared_ptr<Section> section2 = status.GetValue();
  ASSERT_TRUE(section2);

  EXPECT_EQ(section2->at("Email"), "test@gmail.com");
  EXPECT_EQ(section2->at("KeyFilePath"), "C:\\path\\to\\abc");
  EXPECT_EQ(section2->at("OAuthMechanism"), "0");
  EXPECT_EQ(section2->at("Catalog"), kCatalog);
  EXPECT_EQ(section2->at("Dataset"), kDataset);

  result = RemoveDSNFromRegistry(kTestDsn);
  ASSERT_TRUE(result.ok());
}

TEST(RemoveDSNFromRegistry, successDeletion) {
  Section section = CreateTestSection();
  StatusRecord result = AddDSNToRegistry(kTestDsn, kDriver, section);
  ASSERT_TRUE(result.ok());
  result = RemoveDSNFromRegistry(kTestDsn);
  ASSERT_TRUE(result.ok());

  auto status = GetSectionWin("SOFTWARE\\ODBC\\ODBC.INI\\" + kTestDsn);
  ASSERT_FALSE(status.Ok());
}

TEST(AddDSNToRegistry, emptyDSN) {
  Section section = {};
  StatusRecord result = AddDSNToRegistry("", kDriver, section);
  EXPECT_THAT(result, StatusRecIs(SQLStates::k_HY000(),
                                  HasSubstr("DSN Name cannot be empty")));
}

TEST(EditDSNInRegistry, nonExistingDSN) {
  Section section = {};
  StatusRecord result = EditDSNInRegistry("", section);
  EXPECT_THAT(result, StatusRecIs(SQLStates::k_HY000(),
                                  HasSubstr("DSN Name cannot be empty")));
}

TEST(RemoveDSNFromRegistry, nonExistentDSN) {
  StatusRecord result = RemoveDSNFromRegistry("");
  EXPECT_THAT(result,
              StatusRecIs(SQLStates::k_HY000(),
                          HasSubstr("Failed to remove registry key for DSN")));
}

TEST(ConvertLPCSTRToString, valid_string) {
  LPCSTR lpszAttributes =
      "DSN=Personnel Data\0UID=Smith\0PWD=Sesame\0DATABASE=Personnel\0\0";
  std::string result = ConvertLPCSTRToString(lpszAttributes);
  EXPECT_EQ(result,
            "DSN=Personnel Data;UID=Smith;PWD=Sesame;DATABASE=Personnel;;");
  EXPECT_EQ(result.length(), 60);
}

TEST(ConvertLPCSTRToString, invalid_string) {
  LPCSTR lpszAttributes = nullptr;
  std::string result = ConvertLPCSTRToString(lpszAttributes);
  EXPECT_EQ(result, "");
  EXPECT_EQ(result.length(), 0);
}
TEST(ParseConnectionString, null_terminating_string) {
  LPCSTR lpszAttributes =
      "DSN=Personnel Data\0UID=Smith\0PWD=Sesame\0DATABASE=Personnel\0\0";
  std::string conn_str = ConvertLPCSTRToString(lpszAttributes);
  StatusRecordOr<Section> section_resp_status = ParseConnectionString(conn_str);
  ASSERT_STATUS_RECORD_OK(section_resp_status);
}

#endif  //_WIN32
}  // namespace google::cloud::odbc_bq_driver_internal
