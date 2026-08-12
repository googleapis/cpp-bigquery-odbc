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
#include <filesystem>
#include <random>
#include <thread>

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;
using google::cloud::odbc_testing_utils::StatusRecordIs;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;
namespace fs = std::filesystem;

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
    {"Description", "BigQuery ODBC Connector"},
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

TEST(StringUtils, DoubleStrToIntBasic) {
  std::string str = "123.000";
  StatusRecord status = DoubleStrToInt(str);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(str, "123");
}

TEST(StringUtils, DoubleStrToIntFailure) {
  std::string str = "a123";
  StatusRecord status = DoubleStrToInt(str);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(str, "a123");
}

TEST(StringUtils, DoubleStrToIntNullStr) {
  std::string str;
  StatusRecord status = DoubleStrToInt(str);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(str, "");
}

TEST(StringUtils, SplitBasic) {
  std::string s = "SOFTWARE\\ODBC\\ODBC.INI";
  std::vector<std::string> v = Split(s, "\\", 2);
  std::vector<std::string> v_expected{"SOFTWARE", "ODBC\\ODBC.INI"};
  ASSERT_EQ(v_expected.size(), v.size());
  for (int i = 0; i < v_expected.size(); i++) {
    ASSERT_EQ(v_expected[i], v[i]);
  }
}

TEST(StringUtils, SplitDefaultParams) {
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

TEST(StringUtils, JoinBasic) {
  std::vector<std::string> v{"SOFTWARE", "ODBC", "ODBC.INI"};
  std::string s_expected_1 = "ODBC\\ODBC.INI";
  std::string s = Join(v, "\\", 1);
  EXPECT_EQ(s_expected_1, s);

  std::string s_expected_0 = "SOFTWARE\\ODBC\\ODBC.INI";
  s = Join(v, "\\", 0);
  EXPECT_EQ(s_expected_0, s);
}

TEST(StringUtils, JoinDefaultParams) {
  std::vector<std::string> v{"SOFTWARE", "ODBC", "ODBC.INI"};
  std::string s_expected = "SOFTWAREODBCODBC.INI";
  std::string s = Join(v);
  EXPECT_EQ(s_expected, s);
}

TEST(StringUtils, JoinStartIndOutOfRange) {
  std::vector<std::string> v{"SOFTWARE", "ODBC", "ODBC.INI"};
  std::string s_expected;
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

TEST(Parsing, ParseConfigIncorrectPath) {
#ifdef _WIN32
#ifdef _WIN64
  auto sections = ParseConfig("SOFTWARE\\ODBC\\ODBC1.INI");
#else
  auto sections = ParseConfig("SOFTWARE\\WOW6432Node\\ODBC\\ODBC1.INI");
#endif
#else
  auto sections = ParseConfig("/invalid_file_name.ini");
#endif  // _WIN32
  EXPECT_TRUE((*sections)->empty());
}

#ifndef _WIN32
TEST(GetPathToOdbcIni, GetPathEnvVar) {
  std::string expected = "my_path";
  google::cloud::odbc_bigquery_client_interface::SetEnv("ODBCINI", expected);

  std::string actual = GetPathToOdbcIni();

  EXPECT_EQ(actual, expected);
  google::cloud::odbc_bigquery_client_interface::UnsetEnv("ODBCINI");
}
#endif

TEST(GetPathToOdbcIni, GetPathHomeVar) {
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

TEST(GetOdbcTraceConfigPath, GetDefaultPath) {
  auto home = ::google::cloud::internal::GetEnv("GOOGLEBIGQUERYODBCINI");
  // Need to remove the environment variable to use the default path
  google::cloud::odbc_bigquery_client_interface::UnsetEnv(
      "GOOGLEBIGQUERYODBCINI");

  std::string actual = GetOdbcTraceConfigPath();
  EXPECT_EQ(actual, "/etc/googlebigqueryodbc.ini");
  google::cloud::odbc_bigquery_client_interface::SetEnv("GOOGLEBIGQUERYODBCINI",
                                                        home);
}

TEST(GetOdbcTraceConfigPath, GetGoogleODBCIniPath) {
  google::cloud::odbc_bigquery_client_interface::SetEnv(
      "GOOGLEBIGQUERYODBCINI", "/path/to/googleodbcfile.ini");

  std::string actual = GetOdbcTraceConfigPath();
  EXPECT_EQ(actual, "/path/to/googleodbcfile.ini");

  google::cloud::odbc_bigquery_client_interface::UnsetEnv(
      "GOOGLEBIGQUERYODBCINI");
}

TEST(GetDefaultPemFile, NonWinPemFile) {
  Dl_info info{};
  ASSERT_NE(dladdr(reinterpret_cast<void*>(&GetDefaultPemFile), &info), 0);

  fs::path base = fs::path(info.dli_fname).parent_path();
  fs::path expected = base / "roots.pem";

  std::string actual = GetDefaultPemFile();
  EXPECT_EQ(actual, expected.string());
}

#endif  // _WIN32

#ifdef _WIN32
TEST(GetOdbcTraceConfigPath, GetWinRegpath_64bit) {
  std::string actual = GetOdbcTraceConfigPath();
  EXPECT_EQ(actual, k_trace_reg_path);
}

TEST(GetDefaultPemFile, WinPemFilePath) {
  HMODULE hm = nullptr;
  ASSERT_TRUE(
      GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                        reinterpret_cast<LPCSTR>(&GetDefaultPemFile), &hm));

  char path[MAX_PATH];
  ASSERT_NE(GetModuleFileNameA(hm, path, MAX_PATH), 0);

  fs::path base = fs::path(path).parent_path();
  fs::path expected = base / "assets" / "roots.pem";

  std::string actual = GetDefaultPemFile();
  EXPECT_EQ(actual, expected.string());
}
#endif  // _WIN32

TEST(Parsing, ParseConnectionString) {
  Section testing_section = kDsnSection;
  std::string conn_str;
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

TEST(Parsing, ParseConnectionStringInvalidString) {
  Section testing_section = kDsnSection;
  std::string conn_str = "a=3;b;";
  StatusRecordOr<Section> section_resp = ParseConnectionString(conn_str);
  EXPECT_THAT(section_resp,
              StatusRecordIs(SQLStates::k_HY000(), HasSubstr("Invalid")));
}

TEST(Parsing, ParseConnectionStringDuplicateFields) {
  Section testing_section = kDsnSection;
  std::string conn_str = "a=3;a=4;";
  StatusRecordOr<Section> section_resp_status = ParseConnectionString(conn_str);
  ASSERT_STATUS_RECORD_OK(section_resp_status);
  Section section_resp = *section_resp_status;
  EXPECT_EQ(section_resp["a"], "3");
}

TEST(Parsing, ParseConnectionStringRemoveCurlyBraces) {
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
  re2::RE2 odbc_regex(regex);
  EXPECT_TRUE(re2::RE2::FullMatch("abcde", odbc_regex));
  EXPECT_FALSE(re2::RE2::FullMatch("abcd", odbc_regex));
  EXPECT_FALSE(re2::RE2::FullMatch("abcd1", odbc_regex));
}

TEST(FilterUsingOdbcRegex, UsePercent) {
  auto regex = CastOdbcRegexToCppRegex("%abc%");

  EXPECT_EQ(".*abc.*", regex);
  re2::RE2 odbc_regex(regex);
  EXPECT_TRUE(re2::RE2::FullMatch("abcde", odbc_regex));
  EXPECT_TRUE(re2::RE2::FullMatch("abc", odbc_regex));
  EXPECT_TRUE(re2::RE2::FullMatch("00abc", odbc_regex));
  EXPECT_FALSE(re2::RE2::FullMatch("ab1c", odbc_regex));
}

TEST(FilterUsingOdbcRegex, UsePercentWithEscapeCharacter) {
  auto regex = CastOdbcRegexToCppRegex("%a%b\\%c%");

  EXPECT_EQ(".*a.*b%c.*", regex);
  re2::RE2 odbc_regex(regex);
  EXPECT_TRUE(re2::RE2::FullMatch("ab%cde", odbc_regex));
  EXPECT_TRUE(re2::RE2::FullMatch("ab%c", odbc_regex));
  EXPECT_FALSE(re2::RE2::FullMatch("00abc", odbc_regex));
  EXPECT_FALSE(re2::RE2::FullMatch("ab1c", odbc_regex));
}

TEST(FilterUsingOdbcRegex, UseUnderscore) {
  auto regex = CastOdbcRegexToCppRegex("_ab_c_");

  EXPECT_EQ(".ab.c.", regex);
  re2::RE2 odbc_regex(regex);
  EXPECT_TRUE(re2::RE2::FullMatch("0ab1c2", odbc_regex));
  EXPECT_TRUE(re2::RE2::FullMatch("_ab_c_", odbc_regex));
  EXPECT_FALSE(re2::RE2::FullMatch("abc", odbc_regex));
  EXPECT_FALSE(re2::RE2::FullMatch("ab0c", odbc_regex));
}

TEST(FilterUsingOdbcRegex, UseUnderscoreWithEscapeCharacter) {
  auto regex = CastOdbcRegexToCppRegex("_ab\\_c_");

  EXPECT_EQ(".ab_c.", regex);
  re2::RE2 odbc_regex(regex);
  EXPECT_TRUE(re2::RE2::FullMatch("0ab_c2", odbc_regex));
  EXPECT_TRUE(re2::RE2::FullMatch("_ab_c_", odbc_regex));
  EXPECT_FALSE(re2::RE2::FullMatch("0ab1c2", odbc_regex));
  EXPECT_FALSE(re2::RE2::FullMatch("ab_c", odbc_regex));
}

TEST(FilterUsingOdbcRegex, UseComplexPattern) {
  auto regex = CastOdbcRegexToCppRegex("\\%abc\\_def_ghi%");

  EXPECT_EQ("%abc_def.ghi.*", regex);
  re2::RE2 odbc_regex(regex);
  EXPECT_TRUE(re2::RE2::FullMatch("%abc_def0ghi", odbc_regex));
  EXPECT_TRUE(re2::RE2::FullMatch("%abc_def0ghi11111", odbc_regex));
  EXPECT_TRUE(re2::RE2::FullMatch("%abc_def_ghi___", odbc_regex));
  EXPECT_FALSE(re2::RE2::FullMatch("abc_def0ghi", odbc_regex));
  EXPECT_FALSE(re2::RE2::FullMatch("%abc_defghi", odbc_regex));
  EXPECT_FALSE(re2::RE2::FullMatch("%abc_def00ghi", odbc_regex));
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

TEST(UnicodeConversion, SuccessBqConvertSQLWCHARToString) {
  std::wstring query(
      L"INSERT INTO INTEGRATION_TESTS.Test_Table VALUES(4, 'अच्छा', 28)");
  std::vector<SQLWCHAR> sql_w_str(query.begin(), query.end());
  sql_w_str.emplace_back(L'\0');

  SQLWCHAR* statement_text = sql_w_str.data();

  SQLINTEGER length = sql_w_str.size();

  auto result_str = BqConvertSQLWCHARToString(statement_text, length);

  EXPECT_STREQ("INSERT INTO INTEGRATION_TESTS.Test_Table VALUES(4, 'अच्छा', 28)",
               result_str->c_str());
  auto result_wstr = Utf8ToUtf16(*result_str);
  EXPECT_STREQ(query.data(), result_wstr->data());
}

TEST(BqConvertSQLWCHARToString, SuccessEmptystring) {
  std::wstring str;
  std::vector<SQLWCHAR> sql_w_str(str.begin(), str.end());
  sql_w_str.emplace_back(L'\0');

  SQLWCHAR* statement_text = sql_w_str.data();

  SQLINTEGER length = sql_w_str.size();

  auto result_str = BqConvertSQLWCHARToString(statement_text, length);

  EXPECT_STREQ("", result_str->c_str());
}

TEST(UnicodeConversion, SuccessUtf16ToUtf8) {
  std::wstring wstr = L"आपका स्वागत है";
  std::vector<wchar_t> sql_w_str(wstr.begin(), wstr.end());
  sql_w_str.emplace_back(L'\0');
  auto result_str = Utf16ToUtf8(sql_w_str.data());
  ASSERT_FALSE(result_str->empty());
  auto result_wstr = Utf8ToUtf16(*result_str);
  EXPECT_STREQ(sql_w_str.data(), result_wstr->data());
}

TEST(UnicodeConversion, SuccessUtf16ToUtf8Chinese) {
  std::wstring wstr = L"你好，先生，你好吗";
  std::vector<wchar_t> sql_w_str(wstr.begin(), wstr.end());
  sql_w_str.emplace_back(L'\0');
  auto result_str = Utf16ToUtf8(sql_w_str.data());
  ASSERT_FALSE(result_str->empty());
  auto result_wstr = Utf8ToUtf16(*result_str);
  EXPECT_STREQ(sql_w_str.data(), result_wstr->data());
}

TEST(UnicodeConversion, EmptyDataUtf16ToUtf8) {
  std::wstring wstr;
  std::vector<wchar_t> sql_w_str(wstr.begin(), wstr.end());
  sql_w_str.emplace_back(L'\0');
  auto result_str = Utf16ToUtf8(sql_w_str.data());
  ASSERT_TRUE(result_str->empty());
}

TEST(DiagIdentifierString, IsDiagIdentifierStringTrue) {
  EXPECT_TRUE(IsDiagIdentifierString(SQL_DIAG_DYNAMIC_FUNCTION));
  EXPECT_TRUE(IsDiagIdentifierString(SQL_DIAG_CONNECTION_NAME));
  EXPECT_TRUE(IsDiagIdentifierString(SQL_DIAG_SERVER_NAME));
}

TEST(DiagIdentifierString, IsDiagIdentifierStringFalse) {
  EXPECT_FALSE(IsDiagIdentifierString(SQL_DIAG_DYNAMIC_FUNCTION_CODE));
}

TEST(IsFieldIdentifierString, IsFieldIdentifierStringTrue) {
  EXPECT_TRUE(IsFieldIdentifierString(SQL_DESC_BASE_COLUMN_NAME));
  EXPECT_TRUE(IsFieldIdentifierString(SQL_DESC_BASE_TABLE_NAME));
  EXPECT_TRUE(IsFieldIdentifierString(SQL_DESC_CATALOG_NAME));
}

TEST(IsFieldIdentifierString, IsFieldIdentifierStringFalse) {
  EXPECT_FALSE(IsFieldIdentifierString(SQL_DESC_MAXIMUM_SCALE));
}

TEST(ParseStringToInteger, ParseStringToIntegerValid) {
  std::string str = "16384";
  auto status = ParseStringToInteger(str);
  EXPECT_EQ(*status, 16384);
}

TEST(ParseStringToInteger, ParseStringToIntegerLargeValue) {
  std::string str = "4294967296";
  auto status = ParseStringToInteger(str);
  EXPECT_THAT(status,
              StatusRecordIs(SQLStates::k_HY000(),
                             HasSubstr("Input value value is too large")));
}

TEST(ParseStringToInteger, ParseStringToIntegerInvalid) {
  std::string str = "abc";
  auto status = ParseStringToInteger(str);
  EXPECT_THAT(status,
              StatusRecordIs(SQLStates::k_HY000(),
                             HasSubstr("Input value must be an integer")));
}

TEST(IsInfoTypeString, IsInfoTypeStringTrue) {
  EXPECT_TRUE(IsInfoTypeString(SQL_CATALOG_NAME));
  EXPECT_TRUE(IsInfoTypeString(SQL_CATALOG_NAME_SEPARATOR));
  EXPECT_TRUE(IsInfoTypeString(SQL_COLLATION_SEQ));
}

TEST(IsInfoTypeString, IsInfoTypeStringFalse) {
  EXPECT_FALSE(IsInfoTypeString(SQL_INDEX_KEYWORDS));
}

TEST(CheckTargetType, CheckTargetTypeTrue) {
  EXPECT_TRUE(CheckTargetType(SQL_C_CHAR));
  EXPECT_TRUE(CheckTargetType(SQL_C_FLOAT));
  EXPECT_TRUE(CheckTargetType(SQL_C_TYPE_DATE));
}

TEST(CheckTargetType, CheckTargetTypeFalse) {
  EXPECT_FALSE(CheckTargetType(SQL_C_DATE));
}

TEST(IsSearchPatternArgument, SearchPatternPercent) {
  EXPECT_TRUE(IsSearchPatternArgument("%"));
}

TEST(IsSearchPatternArgument, SearchPatternUnderscore) {
  EXPECT_TRUE(IsSearchPatternArgument("_"));
}

TEST(IsSearchPatternArgument, SearchPatternEscape) {
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

TEST(SanitizeIdentifierArgument, QuotedArgumentSingleQuote) {
  std::string arg("      'test'     ");
  SanitizeIdentifierArgument(arg);
  EXPECT_EQ(arg, "test");
}

TEST(SanitizeIdentifierArgument, QuotedArgumentDoubleQuotes) {
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

TEST(PopulateOutputConnectionString, FailTruncated) {
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
  std::string conn_string;

  auto result = PopulateOutputConnectionString(
      out_conn_str, sizeof(out_conn_str), &out_conn_str_len, conn_string);

  EXPECT_EQ(result.ok(), false);
  EXPECT_EQ(result.sql_state, SQLStates::k_HY000());
  EXPECT_EQ(result.message, "Invalid Connection String");
}

TEST(Base64Encode, Success) {
  // Empty input (nullptr, length 0)
  EXPECT_EQ(Base64Encode(nullptr, 0), "");

  // Empty string
  EXPECT_EQ(Base64Encode(reinterpret_cast<uint8_t const*>(""), 0), "");

  // Single character
  EXPECT_EQ(Base64Encode(reinterpret_cast<uint8_t const*>("A"), 1), "QQ==");

  // Two characters
  EXPECT_EQ(Base64Encode(reinterpret_cast<uint8_t const*>("AB"), 2), "QUI=");

  // Three characters
  EXPECT_EQ(Base64Encode(reinterpret_cast<uint8_t const*>("ABC"), 3), "QUJD");

  // Four characters
  EXPECT_EQ(Base64Encode(reinterpret_cast<uint8_t const*>("ABCD"), 4),
            "QUJDRA==");

  // Standard example
  EXPECT_EQ(Base64Encode(reinterpret_cast<uint8_t const*>("Man"), 3), "TWFu");

  // Binary data
  EXPECT_EQ(Base64Encode(reinterpret_cast<uint8_t const*>("\xFF\xEE\xDD"), 3),
            "/+7d");
}

TEST(Base64Encode, Failure) {
  EXPECT_NE(Base64Encode(reinterpret_cast<uint8_t const*>("ABC"), 3),
            "WRONG_OUTPUT");
  EXPECT_NE(Base64Encode(reinterpret_cast<uint8_t const*>("XYZ"), 3), "WFlh");
}

#ifdef _WIN32
std::string kLogLevel = "6";
std::string kLogPath = "/path/to/log";

Section CreateTracelogTestSection() {
  Section section;
  section["LogLevel"] = kLogLevel;
  section["LogPath"] = kLogPath;
  return section;
}

TEST(AddLogTraceToRegistry, Success) {
  auto section = CreateTracelogTestSection();
  StatusRecord result = AddLogTraceToRegistry(section);
  ASSERT_TRUE(result.ok());

  auto status = GetSectionWin(GetOdbcTraceConfigPath() + "\\Driver");
  std::shared_ptr<Section> section2 = status.GetValue();
  ASSERT_TRUE(section2);

  EXPECT_EQ(section2->at("LogLevel"), kLogLevel);
  EXPECT_EQ(section2->at("LogPath"), kLogPath);
}

TEST(ConvertLPCSTRToString, ValidString) {
  LPCSTR lpszAttributes =
      "DSN=Personnel Data\0UID=Smith\0PWD=Sesame\0DATABASE=Personnel\0\0";
  std::string result = ConvertLPCSTRToString(lpszAttributes);
  EXPECT_EQ(result,
            "DSN=Personnel Data;UID=Smith;PWD=Sesame;DATABASE=Personnel;;");
  EXPECT_EQ(result.length(), 60);
}

TEST(ConvertLPCSTRToString, InvalidString) {
  LPCSTR lpszAttributes = nullptr;
  std::string result = ConvertLPCSTRToString(lpszAttributes);
  EXPECT_EQ(result, "");
  EXPECT_EQ(result.length(), 0);
}
TEST(ParseConnectionString, NullTerminatingString) {
  LPCSTR lpszAttributes =
      "DSN=Personnel Data\0UID=Smith\0PWD=Sesame\0DATABASE=Personnel\0\0";
  std::string conn_str = ConvertLPCSTRToString(lpszAttributes);
  StatusRecordOr<Section> section_resp_status = ParseConnectionString(conn_str);
  ASSERT_STATUS_RECORD_OK(section_resp_status);
}

#endif  //_WIN32

TEST(ParseQueryPropertiesTest, EmptyString) {
  auto result = ParseQueryProperties("");
  ASSERT_STATUS_RECORD_OK(result);
  EXPECT_TRUE(result->empty());
}

TEST(ParseQueryPropertiesTest, StringWithOnlySpaces) {
  auto result = ParseQueryProperties("   ");
  ASSERT_STATUS_RECORD_OK(result);
  EXPECT_TRUE(result->empty());
}

TEST(ParseQueryPropertiesTest, SingleValidProperty) {
  auto result = ParseQueryProperties("key1=value1");
  ASSERT_STATUS_RECORD_OK(result);
  ASSERT_EQ(result->size(), 1);
  EXPECT_EQ((*result)[0].key, "key1");
  EXPECT_EQ((*result)[0].value, "value1");
}

TEST(ParseQueryPropertiesTest, MultipleValidProperties) {
  auto result = ParseQueryProperties("key1=value1,key2=value2,key3=value3");
  ASSERT_STATUS_RECORD_OK(result);
  ASSERT_EQ(result->size(), 3);
  EXPECT_EQ((*result)[0].key, "key1");
  EXPECT_EQ((*result)[0].value, "value1");
  EXPECT_EQ((*result)[1].key, "key2");
  EXPECT_EQ((*result)[1].value, "value2");
  EXPECT_EQ((*result)[2].key, "key3");
  EXPECT_EQ((*result)[2].value, "value3");
}

TEST(ParseQueryPropertiesTest, PropertiesWithSpacesAroundCommaAndEquals) {
  auto result = ParseQueryProperties("  key1 = value1  ,  key2  =  value2  ");
  ASSERT_STATUS_RECORD_OK(result);
  ASSERT_EQ(result->size(), 2);
  EXPECT_EQ((*result)[0].key, "key1");
  EXPECT_EQ((*result)[0].value, "value1");
  EXPECT_EQ((*result)[1].key, "key2");
  EXPECT_EQ((*result)[1].value, "value2");
}
TEST(ParseQueryPropertiesTest, MalformedSemicolonSeparator) {
  auto result = ParseQueryProperties("key1=value1;key2=value2");
  EXPECT_THAT(result,
              StatusRecordIs(SQLStates::k_HY000(),
                             HasSubstr("Malformed list of key-value pairs. "
                                       "Multiple properties not separated by a "
                                       "comma (,).")));
}
TEST(ParseQueryPropertiesTest, MalformedMissingProperty) {
  auto result = ParseQueryProperties("key1=value1,,key2=value2");
  EXPECT_THAT(result,
              StatusRecordIs(SQLStates::k_HY000(),
                             HasSubstr("Malformed list of key-value pairs. "
                                       "Property not separated by an equals "
                                       "sign (=).")));
}
TEST(ParseQueryPropertiesTest, MissingEquals) {
  auto result = ParseQueryProperties("key1value1");
  EXPECT_THAT(result, StatusRecordIs(SQLStates::k_HY000(),
                                     HasSubstr("Invalid Query Property Format: "
                                               "Missing '=' or value")));
}
TEST(ParseQueryPropertiesTest, MultiplePropertiesOneEmptyValue) {
  auto result = ParseQueryProperties("key1=value1,key2=");
  EXPECT_THAT(
      result,
      StatusRecordIs(
          SQLStates::k_HY000(),
          HasSubstr(
              "Invalid Query Property Format: Empty value for key 'key2'")));
}

TEST(GetLocationfromPSC, ValidRegion) {
  std::string psc = "BIGQUERY=https://us-east4-bigquery.googleapis.com/";
  EXPECT_EQ(GetLocationfromPSC(psc), "us-east4");
}

TEST(GetLocationfromPSC, ValidRegionWithExtraParams) {
  std::string psc =
      "BIGQUERY=https://europe-west1-bigquery.googleapis.com/;other=param";
  EXPECT_EQ(GetLocationfromPSC(psc), "europe-west1");
}

TEST(GetLocationfromPSC, MultipleKeysTakesFirst) {
  std::string psc =
      "BIGQUERY=https://us-central1-bigquery.googleapis.com/;"
      "BIGQUERY=https://asia-southeast1-bigquery.googleapis.com/";
  EXPECT_EQ(GetLocationfromPSC(psc), "us-central1");
}

TEST(GetLocationfromPSC, MissingKeyReturnsEmpty) {
  std::string psc = "https://us-east4-bigquery.googleapis.com/";
  EXPECT_EQ(GetLocationfromPSC(psc), "");
}

TEST(GetLocationfromPSC, MissingSuffixReturnsEmpty) {
  std::string psc = "BIGQUERY=https://us-east4.googleapis.com/";
  EXPECT_EQ(GetLocationfromPSC(psc), "");
}

TEST(GetLocationfromPSC, EmptyStringReturnsEmpty) {
  EXPECT_EQ(GetLocationfromPSC(""), "");
}

TEST(GetLocationfromPSC, KeyPresentButMalformed) {
  std::string psc = "BIGQUERY=https://-bigquery.googleapis.com/";
  EXPECT_EQ(GetLocationfromPSC(psc), "");
}

TEST(GetLocationfromPSC, HandlesExtraWhitespace) {
  std::string psc =
      "  BIGQUERY=https://asia-northeast1-bigquery.googleapis.com/  ";
  EXPECT_EQ(GetLocationfromPSC(psc), "asia-northeast1");
}

TEST(ExecuteParallelTasksTest, SuccessWithMultipleThreads) {
  // Input: A list of integers
  std::vector<std::uint32_t> inputs = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

  // Task: Square the number
  auto square_task = [](int input) -> StatusRecordOr<int> {
    return input * input;
  };

  // Execute with fewer threads than tasks to force queuing/sliding window
  std::uint32_t max_threads = 3;
  // Explicitly specify <TaskInput, TaskResult>
  auto result = ExecuteParallelTasks<std::uint32_t, int>(max_threads, inputs,
                                                         square_task);

  ASSERT_STATUS_RECORD_OK(result);
  // Order is not guaranteed due to parallelism, so we use UnorderedElementsAre
  EXPECT_THAT(*result,
              UnorderedElementsAre(1, 4, 9, 16, 25, 36, 49, 64, 81, 100));
}

TEST(ExecuteParallelTasksTest, SuccessWithSingleThread) {
  std::vector<std::string> inputs = {"a", "b", "c"};

  auto append_task = [](std::string const& s) -> StatusRecordOr<std::string> {
    return s + "!";
  };

  // Execute with 1 thread (serial execution)
  auto result =
      ExecuteParallelTasks<std::string, std::string>(1, inputs, append_task);

  ASSERT_STATUS_RECORD_OK(result);
  EXPECT_THAT(*result, UnorderedElementsAre("a!", "b!", "c!"));
}

TEST(ExecuteParallelTasksTest, HandlesEmptyInput) {
  std::vector<int> inputs;
  auto dummy_task = [](int i) -> StatusRecordOr<int> { return i; };

  auto result = ExecuteParallelTasks<int, int>(5, inputs, dummy_task);

  ASSERT_STATUS_RECORD_OK(result);
  EXPECT_THAT(*result, IsEmpty());
}

TEST(ExecuteParallelTasksTest, ReturnsErrorOnTaskFailure) {
  std::vector<int> inputs = {1, 2, 0, 4};  // 0 triggers failure

  auto potentially_failing_task = [](int input) -> StatusRecordOr<int> {
    if (input == 0) {
      return StatusRecord{SQLStates::k_HY000(), "Input cannot be zero"};
    }
    return input * 2;
  };

  auto result =
      ExecuteParallelTasks<int, int>(4, inputs, potentially_failing_task);

  ASSERT_FALSE(result.Ok());
  EXPECT_THAT(result.GetStatusRecord().message,
              HasSubstr("Input cannot be zero"));
}

TEST(ExecuteParallelTasksTest, DrainsThreadsAfterError) {
  // This test ensures that if an error occurs, the utility doesn't crash
  // or hang, but finishes cleaning up active threads.
  std::vector<int> inputs = {1, 2, 3};

  auto slow_failing_task = [](int input) -> StatusRecordOr<int> {
    if (input == 2) {
      // Fail quickly
      return StatusRecord{SQLStates::k_HY000(), "Failed"};
    }
    // Sleep to ensure this thread is still "running" when the error happens
    // elsewhere
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return input;
  };

  // Max threads 3, so all launch roughly at once. Input 2 fails.
  auto result = ExecuteParallelTasks<int, int>(3, inputs, slow_failing_task);

  ASSERT_FALSE(result.Ok());
  EXPECT_EQ(result.GetStatusRecord().message, "Failed");
}

TEST(ExecuteParallelTasksTest, RespectsSlidingWindow) {
  // This test confirms that tasks are not all launched at once if max_threads
  // is limited.

  int task_count = 6;
  std::uint32_t max_threads = 2;
  int min_sleep_ms = 50;
  int max_sleep_ms = 100;

  std::vector<std::uint32_t> inputs(task_count, 0);

  auto start_time = std::chrono::high_resolution_clock::now();

  auto sleeping_task = [min_sleep_ms,
                        max_sleep_ms](int) -> StatusRecordOr<int> {
    // We create a local generator to be thread-safe without locking
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(min_sleep_ms, max_sleep_ms);

    int sleep_time = dist(gen);
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
    return 0;
  };

  auto result = ExecuteParallelTasks<std::uint32_t, int>(max_threads, inputs,
                                                         sleeping_task);

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                      end_time - start_time)
                      .count();

  ASSERT_STATUS_RECORD_OK(result);

  // Validation Logic:
  // 1. We have 6 tasks and 2 threads.
  // 2. The minimum theoretical serial time for one thread to do all work = 6 *
  // 50ms = 300ms.
  // 3. Distributed perfectly over 2 threads = 300ms / 2 = 150ms.
  // 4. If `max_threads` was ignored (infinite threads), execution would take
  // max(random_sleeps) <= 100ms.
  //
  // Therefore, if duration >= 150ms (approx), we know we throttled execution.
  // We use integer math (task_count / max_threads) to represent the number of
  // sequential batches.

  EXPECT_GE(duration, (task_count / max_threads) * min_sleep_ms);
}

}  // namespace google::cloud::odbc_bq_driver_internal
