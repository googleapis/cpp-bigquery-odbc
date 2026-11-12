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

// clang-format off
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
// clang-format on

#include "google/cloud/odbc/bq_driver/internal/utils.h"
#include "google/cloud/odbc/bq_client_interface/setenv.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include "google/cloud/internal/getenv.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <random>
#include <thread>

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::odbc_bigquery_client_interface::SetEnv;
using ::google::cloud::odbc_bigquery_client_interface::UnsetEnv;
using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;
using google::cloud::odbc_testing_utils::StatusRecordIs;
using ::testing::ElementsAre;
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

TEST(ParseStringToInt64, ParseStringToInt64Valid) {
  // A value beyond the range of SQLUINTEGER, which is why this overload exists.
  std::string str = "1099511627776";
  auto status = ParseStringToInt64(str);
  EXPECT_EQ(*status, 1099511627776);
}

TEST(ParseStringToInt64, ParseStringToInt64Empty) {
  std::string str;
  auto status = ParseStringToInt64(str);
  EXPECT_EQ(*status, 0);
}

TEST(ParseStringToInt64, ParseStringToInt64TooLarge) {
  std::string str = "99999999999999999999";
  auto status = ParseStringToInt64(str);
  EXPECT_THAT(status,
              StatusRecordIs(SQLStates::k_HY000(),
                             HasSubstr("Input value value is too large")));
}

TEST(ParseStringToInt64, ParseStringToInt64Invalid) {
  std::string str = "abc";
  auto status = ParseStringToInt64(str);
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

TEST(ExecuteParallelTasksTest, ZeroMaxThreadsRunsSerially) {
  // A misconfigured MaxThreads of 0 must not hang; it degrades to serial
  // execution.
  std::vector<int> inputs = {1, 2, 3};
  auto task = [](int input) -> StatusRecordOr<int> { return input * 2; };

  auto result = ExecuteParallelTasks<int, int>(0, inputs, task);

  ASSERT_STATUS_RECORD_OK(result);
  EXPECT_THAT(*result, ElementsAre(2, 4, 6));
}

TEST(EscapeOdbcPattern, PlainNameUnchanged) {
  EXPECT_EQ(EscapeOdbcPattern("kirltest"), "kirltest");
}

TEST(EscapeOdbcPattern, EscapesUnderscores) {
  // '_' is an ODBC single-character wildcard; a configured dataset name
  // containing it must match only itself.
  EXPECT_EQ(EscapeOdbcPattern("ODBC_TEST_DATASET"), "ODBC\\_TEST\\_DATASET");
}

TEST(EscapeOdbcPattern, EscapesPercentAndBackslash) {
  EXPECT_EQ(EscapeOdbcPattern("a%b"), "a\\%b");
  EXPECT_EQ(EscapeOdbcPattern("a\\b"), "a\\\\b");
}

TEST(EscapeOdbcPattern, EscapedNameMatchesOnlyItself) {
  // The escaped form must compile to a regex that matches the literal name and
  // rejects the wildcard expansions the unescaped form would have accepted.
  auto regex = BuildRegex(EscapeOdbcPattern("ODBC_TEST_DATASET"), SQL_FALSE);
  EXPECT_TRUE(re2::RE2::FullMatch("ODBC_TEST_DATASET", *regex));
  EXPECT_FALSE(re2::RE2::FullMatch("ODBCxTESTyDATASET", *regex));
  EXPECT_FALSE(re2::RE2::FullMatch("ODBC-TEST-DATASET", *regex));

  // Without escaping, '_' acts as a wildcard -- the behaviour being fixed.
  auto unescaped = BuildRegex("ODBC_TEST_DATASET", SQL_FALSE);
  EXPECT_TRUE(re2::RE2::FullMatch("ODBCxTESTyDATASET", *unescaped));
}

TEST(TranslateOdbcEscapeSequences, DatetimeLiterals) {
  EXPECT_EQ(TranslateOdbcEscapeSequences(
                "SELECT {ts '2014-02-20 09:34:06.000'} AS ts_col"),
            "SELECT TIMESTAMP '2014-02-20 09:34:06.000' AS ts_col");
  EXPECT_EQ(TranslateOdbcEscapeSequences(
                "SELECT {TS '2014-02-20 09:34:06'} AS ts_col"),
            "SELECT TIMESTAMP '2014-02-20 09:34:06' AS ts_col");
  EXPECT_EQ(TranslateOdbcEscapeSequences(
                "SELECT * FROM t WHERE d = {d '2023-01-01'}"),
            "SELECT * FROM t WHERE d = DATE '2023-01-01'");
  EXPECT_EQ(TranslateOdbcEscapeSequences(
                "SELECT * FROM t WHERE d = {D '2023-01-01'}"),
            "SELECT * FROM t WHERE d = DATE '2023-01-01'");
  EXPECT_EQ(
      TranslateOdbcEscapeSequences("SELECT * FROM t WHERE tm = {t '12:30:00'}"),
      "SELECT * FROM t WHERE tm = TIME '12:30:00'");
  EXPECT_EQ(
      TranslateOdbcEscapeSequences("SELECT * FROM t WHERE tm = {T '12:30:00'}"),
      "SELECT * FROM t WHERE tm = TIME '12:30:00'");
}

TEST(TranslateOdbcEscapeSequences, EscapeAndOuterJoin) {
  EXPECT_EQ(TranslateOdbcEscapeSequences(
                "SELECT * FROM t WHERE name LIKE '\\%AAA%' {escape '\\'}"),
            "SELECT * FROM t WHERE name LIKE '\\%AAA%'");
  EXPECT_EQ(
      TranslateOdbcEscapeSequences(
          "SELECT * FROM {oj Customers LEFT OUTER JOIN Orders ON c.id=o.id}"),
      "SELECT * FROM Customers LEFT OUTER JOIN Orders ON c.id=o.id");
}

TEST(TranslateOdbcEscapeSequences, PreservesStringsAndCommentsAndStructs) {
  EXPECT_EQ(
      TranslateOdbcEscapeSequences(
          "SELECT '{ts 2020-01-01}' AS col, {ts '2020-01-01 00:00:00'} AS ts"),
      "SELECT '{ts 2020-01-01}' AS col, TIMESTAMP '2020-01-01 00:00:00' AS ts");
  EXPECT_EQ(TranslateOdbcEscapeSequences(
                "SELECT \"{ts '2014-02-20 09:34:06.000'}\" AS s"),
            "SELECT \"{ts '2014-02-20 09:34:06.000'}\" AS s");
  EXPECT_EQ(TranslateOdbcEscapeSequences(
                "SELECT '{ts \\'2014-02-20 09:34:06.000\\'}' AS s"),
            "SELECT '{ts \\'2014-02-20 09:34:06.000\\'}' AS s");
  EXPECT_EQ(
      TranslateOdbcEscapeSequences("SELECT 'escaped \\' {ts 2020} quote' AS s"),
      "SELECT 'escaped \\' {ts 2020} quote' AS s");
  EXPECT_EQ(
      TranslateOdbcEscapeSequences(
          "SELECT 'escaped \\' quote', {ts '2020-01-01 00:00:00'} AS ts"),
      "SELECT 'escaped \\' quote', TIMESTAMP '2020-01-01 00:00:00' AS ts");
  EXPECT_EQ(TranslateOdbcEscapeSequences("SELECT `table_{ts}` FROM tbl"),
            "SELECT `table_{ts}` FROM tbl");
  EXPECT_EQ(TranslateOdbcEscapeSequences("SELECT /* {ts '2020'} */ 1"),
            "SELECT /* {ts '2020'} */ 1");
  EXPECT_EQ(TranslateOdbcEscapeSequences("SELECT -- {ts '2020'}\n 1"),
            "SELECT -- {ts '2020'}\n 1");
  EXPECT_EQ(TranslateOdbcEscapeSequences("SELECT {'a': 1} AS struct_col"),
            "SELECT {'a': 1} AS struct_col");
}

TEST(TranslateOdbcEscapeSequences, InvalidAndMalformedCases) {
  // Unknown escape tag is preserved as-is
  EXPECT_EQ(TranslateOdbcEscapeSequences("SELECT {lb 'abc'}"),
            "SELECT {lb 'abc'}");
  // Unclosed brace is preserved as-is
  EXPECT_EQ(TranslateOdbcEscapeSequences("SELECT {ts '2020'"),
            "SELECT {ts '2020'");
  // Multiple / malformed literals inside {ts} are preserved as-is
  EXPECT_EQ(TranslateOdbcEscapeSequences("SELECT {ts '2020' '2021'}"),
            "SELECT {ts '2020' '2021'}");
  // Tag without whitespace delimiter is preserved as-is
  EXPECT_EQ(TranslateOdbcEscapeSequences("SELECT {ts'2020'}"),
            "SELECT {ts'2020'}");
  // Unquoted literal inside {ts} is preserved as-is
  EXPECT_EQ(TranslateOdbcEscapeSequences("SELECT {ts 2020-01-01}"),
            "SELECT {ts 2020-01-01}");
  // Empty braces are preserved as-is
  EXPECT_EQ(TranslateOdbcEscapeSequences("SELECT {}"), "SELECT {}");
}

// ============================================================================
// DSN Precedence and Resolution Tests
// ============================================================================

TEST(GetDsnSection, EmptyDsnReturnsError) {
  auto res = GetDsnSection("");
  EXPECT_FALSE(res.Ok());
}

#ifndef _WIN32
TEST(GetPathToSystemOdbcIni, EnvVar) {
  std::string expected_dir = "/custom/system/odbc";
  SetEnv("ODBCSYSINI", expected_dir);

  std::string actual = GetPathToSystemOdbcIni();
  EXPECT_EQ(actual, expected_dir + "/odbc.ini");

  UnsetEnv("ODBCSYSINI");
}

TEST(GetPathToSystemOdbcIni, DefaultPath) {
  auto orig = GetEnv("ODBCSYSINI");
  UnsetEnv("ODBCSYSINI");

  std::string actual = GetPathToSystemOdbcIni();
  EXPECT_EQ(actual, "/etc/odbc.ini");

  if (orig) {
    SetEnv("ODBCSYSINI", *orig);
  }
}

TEST(GetDsnSection, LinuxUserDsnPrecedenceOverSystemDsn) {
  auto temp_dir = fs::temp_directory_path() / "odbc_test_precedence";
  fs::create_directories(temp_dir);

  auto user_ini_file = temp_dir / "user_odbc.ini";
  auto sys_ini_file = temp_dir / "odbc.ini";

  {
    std::ofstream user_out(user_ini_file);
    user_out << "[SharedDSN]\n"
             << "Catalog=UserCatalogLinux\n"
             << "OAuthMechanism=1\n";
  }

  {
    std::ofstream sys_out(sys_ini_file);
    sys_out << "[SharedDSN]\n"
            << "Catalog=SystemCatalogLinux\n"
            << "OAuthMechanism=0\n\n"
            << "[SystemOnlyDSN]\n"
            << "Catalog=SystemCatalogOnlyLinux\n"
            << "OAuthMechanism=3\n";
  }

  auto orig_odbcini = GetEnv("ODBCINI");
  auto orig_odbcsysini = GetEnv("ODBCSYSINI");

  SetEnv("ODBCINI", user_ini_file.string());
  SetEnv("ODBCSYSINI", temp_dir.string());

  // 1. User DSN takes precedence over System DSN for SharedDSN
  auto shared_res = GetDsnSection("SharedDSN");
  ASSERT_TRUE(shared_res.Ok());
  EXPECT_EQ((*shared_res)->at("Catalog"), "UserCatalogLinux");
  EXPECT_EQ((*shared_res)->at("OAuthMechanism"), "1");

  // 2. Fall back to System DSN when DSN is absent in User DSN
  auto sys_res = GetDsnSection("SystemOnlyDSN");
  ASSERT_TRUE(sys_res.Ok());
  EXPECT_EQ((*sys_res)->at("Catalog"), "SystemCatalogOnlyLinux");
  EXPECT_EQ((*sys_res)->at("OAuthMechanism"), "3");

  // 3. Error when DSN is in neither
  auto missing_res = GetDsnSection("NonExistentDSN");
  EXPECT_FALSE(missing_res.Ok());

  // Restore environment
  if (orig_odbcini) {
    SetEnv("ODBCINI", *orig_odbcini);
  } else {
    UnsetEnv("ODBCINI");
  }
  if (orig_odbcsysini) {
    SetEnv("ODBCSYSINI", *orig_odbcsysini);
  } else {
    UnsetEnv("ODBCSYSINI");
  }

  std::error_code ec;
  fs::remove_all(temp_dir, ec);
}
#endif  // !_WIN32

#ifdef _WIN32
TEST(GetSectionWin, HiveParameterUserAndSystem) {
  std::string const test_path = "SOFTWARE\\GoogleTest_GetSectionHive";
  Section sec = {{"TestProp", "TestVal"}};

  ASSERT_TRUE(SetRegValues(HKEY_CURRENT_USER, test_path, sec).ok());

  auto res = GetSectionWin(test_path, HKEY_CURRENT_USER);
  ASSERT_TRUE(res.Ok());
  EXPECT_EQ((*res)->at("TestProp"), "TestVal");

  // Clean up
  RegDeleteKeyA(HKEY_CURRENT_USER, test_path.c_str());
}

TEST(GetDsnSectionWin, WindowsUserDsnPrecedenceOverSystemDsn) {
  std::string const dsn_name = "TestPrecedenceDSN";
  std::string const user_base = "SOFTWARE\\GoogleTest_UserODBC";
  std::string const sys_base = "SOFTWARE\\GoogleTest_SysODBC";
  std::string const user_key = user_base + "\\" + dsn_name;
  std::string const sys_key = sys_base + "\\" + dsn_name;

  Section user_sec = {{"Catalog", "UserCatalogWin"}, {"OAuthMechanism", "1"}};
  Section sys_sec = {{"Catalog", "SystemCatalogWin"}, {"OAuthMechanism", "0"}};
  Section sys_only_sec = {{"Catalog", "SystemOnlyCatalogWin"},
                          {"OAuthMechanism", "3"}};

  // Use HKCU for both user and system test subkeys for isolated test execution
  ASSERT_TRUE(SetRegValues(HKEY_CURRENT_USER, user_key, user_sec).ok());
  ASSERT_TRUE(SetRegValues(HKEY_CURRENT_USER, sys_key, sys_sec).ok());
  ASSERT_TRUE(SetRegValues(HKEY_CURRENT_USER, sys_base + "\\SystemOnlyWin",
                           sys_only_sec)
                  .ok());

  // 1. User DSN takes precedence over System DSN
  auto shared_res = GetDsnSectionWin(dsn_name, user_base, sys_base,
                                     HKEY_CURRENT_USER, HKEY_CURRENT_USER);
  ASSERT_TRUE(shared_res.Ok());
  EXPECT_EQ((*shared_res)->at("Catalog"), "UserCatalogWin");
  EXPECT_EQ((*shared_res)->at("OAuthMechanism"), "1");

  // 2. Fall back to System DSN when absent in User DSN
  auto sys_res = GetDsnSectionWin("SystemOnlyWin", user_base, sys_base,
                                  HKEY_CURRENT_USER, HKEY_CURRENT_USER);
  ASSERT_TRUE(sys_res.Ok());
  EXPECT_EQ((*sys_res)->at("Catalog"), "SystemOnlyCatalogWin");
  EXPECT_EQ((*sys_res)->at("OAuthMechanism"), "3");

  // 3. Error when absent in both
  auto missing_res = GetDsnSectionWin("MissingWinDSN", user_base, sys_base,
                                      HKEY_CURRENT_USER, HKEY_CURRENT_USER);
  EXPECT_FALSE(missing_res.Ok());

  // Cleanup
  RegDeleteKeyA(HKEY_CURRENT_USER, user_key.c_str());
  RegDeleteKeyA(HKEY_CURRENT_USER, user_base.c_str());
  RegDeleteKeyA(HKEY_CURRENT_USER, (sys_base + "\\SystemOnlyWin").c_str());
  RegDeleteKeyA(HKEY_CURRENT_USER, sys_key.c_str());
  RegDeleteKeyA(HKEY_CURRENT_USER, sys_base.c_str());
}

TEST(GetDsnSectionWin, NonExistentDsnReturnsError) {
  auto res = GetDsnSectionWin("NonExistentDsn_123456789");
  EXPECT_FALSE(res.Ok());
}
#endif  // _WIN32

namespace {

using odbc_bigquery_client_interface::ConnProps;
using odbc_bigquery_client_interface::OauthMechanism;

std::map<std::string, std::string> ParseUrlQueryForTest(
    std::string const& url) {
  std::map<std::string, std::string> out;
  auto qpos = url.find('?');
  if (qpos == std::string::npos) return out;
  std::string query = url.substr(qpos + 1);
  std::size_t start = 0;
  while (start <= query.size()) {
    auto amp = query.find('&', start);
    std::string kv = (amp == std::string::npos)
                         ? query.substr(start)
                         : query.substr(start, amp - start);
    auto eq = kv.find('=');
    if (eq != std::string::npos) {
      std::string key = kv.substr(0, eq);
      std::string val = kv.substr(eq + 1);
      std::string decoded;
      for (std::size_t i = 0; i < val.size(); ++i) {
        if (val[i] == '%' && i + 2 < val.size()) {
          decoded.push_back(
              static_cast<char>(std::stoi(val.substr(i + 1, 2), nullptr, 16)));
          i += 2;
        } else if (val[i] == '+') {
          decoded.push_back(' ');
        } else {
          decoded.push_back(val[i]);
        }
      }
      out[key] = decoded;
    }
    if (amp == std::string::npos) break;
    start = amp + 1;
  }
  return out;
}

int ExtractPortFromRedirectUri(std::string const& redirect_uri) {
  auto colon = redirect_uri.rfind(':');
  auto slash = redirect_uri.find('/', colon);
  return std::stoi(redirect_uri.substr(colon + 1, slash - colon - 1));
}

std::string SendLoopbackCallbackRequest(int port, std::string const& path,
                                        bool open_preconnect_socket = false) {
#ifdef _WIN32
  WSADATA wsa;
  WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(static_cast<std::uint16_t>(port));

  if (open_preconnect_socket) {
    auto pre_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    static_cast<void>(
        connect(pre_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)));
#ifdef _WIN32
    closesocket(pre_sock);
#else
    close(pre_sock);
#endif
  }

  auto sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  static_cast<void>(
      connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)));
  std::string req = "GET " + path +
                    " HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
  send(sock, req.data(), static_cast<int>(req.size()), 0);
  char buf[4096] = {0};
  int n = recv(sock, buf, sizeof(buf) - 1, 0);
#ifdef _WIN32
  closesocket(sock);
  WSACleanup();
#else
  close(sock);
#endif
  return n > 0 ? std::string(buf, static_cast<std::size_t>(n)) : std::string{};
}

}  // namespace

TEST(PkceHelpers, Rfc7636S256VectorAndVerifierLength) {
  EXPECT_EQ(
      ComputePkceCodeChallenge("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk"),
      "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM");

  std::string verifier = GeneratePkceCodeVerifier();
  EXPECT_GE(verifier.size(), 43U);
  EXPECT_LE(verifier.size(), 128U);
  EXPECT_EQ(verifier.find('='), std::string::npos);
}

TEST(InteractiveOAuthSignIn, StandardGoogleAccountPkceFlow) {
  ClearInteractiveOAuthTokenCache();
  ConnProps props{};
  props.auth_mechanism = OauthMechanism::kUserAccount;
  props.use_default_client_credentials = false;
  props.client_id = "my-installed-client-id";
  props.client_secret = "my-installed-client-secret";

  std::string captured_challenge;
  std::string captured_redirect_uri;
  std::string callback_http_response;
  std::thread client_thread;

  auto launcher = [&](std::string const& url) -> odbc_internal::StatusRecord {
    EXPECT_THAT(url,
                HasSubstr("https://accounts.google.com/o/oauth2/v2/auth?"));
    auto q = ParseUrlQueryForTest(url);
    EXPECT_EQ(q["client_id"], "my-installed-client-id");
    EXPECT_EQ(q["code_challenge_method"], "S256");
    EXPECT_EQ(q["access_type"], "offline");
    EXPECT_EQ(q["prompt"], "consent");
    captured_challenge = q["code_challenge"];
    captured_redirect_uri = q["redirect_uri"];
    int port = ExtractPortFromRedirectUri(captured_redirect_uri);
    std::string state = q["state"];
    client_thread = std::thread([port, state, &callback_http_response]() {
      callback_http_response = SendLoopbackCallbackRequest(
          port, "/?code=mock%2Fauth_code&state=" + state,
          /*open_preconnect_socket=*/true);
    });
    return odbc_internal::StatusRecord::Ok();
  };

  auto post_fn =
      [&](std::string const& token_url,
          std::vector<std::pair<std::string, std::string>> const& headers,
          std::vector<std::pair<std::string, std::string>> const& form_data)
      -> StatusOr<std::string> {
    EXPECT_EQ(token_url, "https://oauth2.googleapis.com/token");
    EXPECT_TRUE(headers.empty());
    std::map<std::string, std::string> form(form_data.begin(), form_data.end());
    EXPECT_EQ(form["grant_type"], "authorization_code");
    EXPECT_EQ(form["code"], "mock/auth_code");
    EXPECT_EQ(form["redirect_uri"], captured_redirect_uri);
    EXPECT_EQ(form["client_id"], "my-installed-client-id");
    EXPECT_EQ(form["client_secret"], "my-installed-client-secret");
    EXPECT_EQ(ComputePkceCodeChallenge(form["code_verifier"]),
              captured_challenge);
    return std::string{
        R"({"access_token":"mock_access_tok","expires_in":3600,"refresh_token":"mock_new_refresh_token","token_type":"Bearer"})"};
  };

  auto res = RunInteractiveOAuthSignIn(props, Options{}, launcher, post_fn,
                                       std::chrono::seconds(5));
  if (client_thread.joinable()) client_thread.join();
  ASSERT_STATUS_RECORD_OK(res);
  EXPECT_FALSE(res->is_workforce);
  EXPECT_EQ(res->refresh_token, "mock_new_refresh_token");
  EXPECT_THAT(callback_http_response, HasSubstr("200 OK"));

  auto cached_res = GetOrRunInteractiveOAuthSignIn(props);
  ASSERT_STATUS_RECORD_OK(cached_res);
  EXPECT_EQ(cached_res->refresh_token, "mock_new_refresh_token");
}

TEST(InteractiveOAuthSignIn,
     WorkforceIdentityFederationExplicitPropsAndLoginConfig) {
  ConnProps props{};
  props.auth_mechanism = OauthMechanism::kExternalUser;
  props.byoid_aud_url =
      "//iam.googleapis.com/locations/global/workforcePools/corp-pool/"
      "providers/okta-oidc";
  props.gcd.universe_domain = "sovereign.example.net";
  props.universe_cloud_web_domain = "cloud.sovereign.example.net";

  std::string captured_challenge;
  std::thread client_thread;
  auto launcher = [&](std::string const& url) -> odbc_internal::StatusRecord {
    EXPECT_THAT(
        url, HasSubstr("https://auth.cloud.sovereign.example.net/authorize?"));
    auto q = ParseUrlQueryForTest(url);
    EXPECT_EQ(q["provider_name"],
              "locations/global/workforcePools/corp-pool/providers/okta-oidc");
    EXPECT_EQ(q["code_challenge_method"], "S256");
    captured_challenge = q["code_challenge"];
    int port = ExtractPortFromRedirectUri(q["redirect_uri"]);
    std::string state = q["state"];
    client_thread = std::thread([port, state]() {
      SendLoopbackCallbackRequest(port, "/?code=wif_code_123&state=" + state);
    });
    return odbc_internal::StatusRecord::Ok();
  };

  auto post_fn =
      [&](std::string const& token_url,
          std::vector<std::pair<std::string, std::string>> const& headers,
          std::vector<std::pair<std::string, std::string>> const& form_data)
      -> StatusOr<std::string> {
    EXPECT_EQ(token_url, "https://sts.sovereign.example.net/v1/oauthtoken");
    EXPECT_EQ(headers.size(), 1U);
    if (!headers.empty()) {
      EXPECT_EQ(headers[0].first, "Authorization");
      EXPECT_THAT(headers[0].second, HasSubstr("Basic "));
    }
    std::map<std::string, std::string> form(form_data.begin(), form_data.end());
    EXPECT_EQ(form["grant_type"], "authorization_code");
    EXPECT_EQ(form["code"], "wif_code_123");
    EXPECT_EQ(form.count("client_id"), 0U);
    EXPECT_EQ(ComputePkceCodeChallenge(form["code_verifier"]),
              captured_challenge);
    return std::string{
        R"({"access_token":"sts_access_tok","expires_in":3600,"refresh_token":"sts_refresh_tok","token_type":"Bearer"})"};
  };

  auto res = RunInteractiveOAuthSignIn(props, Options{}, launcher, post_fn,
                                       std::chrono::seconds(5));
  if (client_thread.joinable()) client_thread.join();
  ASSERT_STATUS_RECORD_OK(res);
  EXPECT_TRUE(res->is_workforce);
  EXPECT_EQ(res->refresh_token, "sts_refresh_tok");
  EXPECT_EQ(res->universe_domain, "sovereign.example.net");
}

TEST(InteractiveOAuthSignIn, WorkforceIdentityFederationLoginConfigFile) {
  std::string const login_config_file =
      "test_external_account_login_config.json";
  {
    std::ofstream ofs(login_config_file);
    ofs << R"({
      "type": "external_account_authorized_user_login_config",
      "audience": "//iam.googleapis.com/locations/global/workforcePools/login-pool/providers/saml-prov",
      "auth_url": "https://auth.cloud.google/authorize",
      "token_url": "https://sts.googleapis.com/v1/oauthtoken",
      "universe_domain": "googleapis.com",
      "quota_project_id": "login-quota-proj"
    })";
  }

  ConnProps props{};
  props.auth_mechanism = OauthMechanism::kUserAccount;
  props.credentials_file_path = login_config_file;

  std::thread client_thread;
  auto launcher = [&](std::string const& url) -> odbc_internal::StatusRecord {
    EXPECT_THAT(url, HasSubstr("https://auth.cloud.google/authorize?"));
    auto q = ParseUrlQueryForTest(url);
    EXPECT_EQ(q["provider_name"],
              "locations/global/workforcePools/login-pool/providers/saml-prov");
    int port = ExtractPortFromRedirectUri(q["redirect_uri"]);
    std::string state = q["state"];
    client_thread = std::thread([port, state]() {
      SendLoopbackCallbackRequest(port, "/?code=login_cfg_code&state=" + state);
    });
    return odbc_internal::StatusRecord::Ok();
  };

  auto post_fn = [&](std::string const& token_url,
                     std::vector<std::pair<std::string, std::string>> const&,
                     std::vector<std::pair<std::string, std::string>> const&)
      -> StatusOr<std::string> {
    EXPECT_EQ(token_url, "https://sts.googleapis.com/v1/oauthtoken");
    return std::string{
        R"({"access_token":"tok1","expires_in":3600,"refresh_token":"rtok_login_cfg","token_type":"Bearer"})"};
  };

  auto sign_in = RunInteractiveOAuthSignIn(props, Options{}, launcher, post_fn,
                                           std::chrono::seconds(5));
  if (client_thread.joinable()) client_thread.join();
  ASSERT_STATUS_RECORD_OK(sign_in);
  EXPECT_TRUE(sign_in->is_workforce);
  EXPECT_EQ(sign_in->refresh_token, "rtok_login_cfg");

  std::string ud;
  std::string user_proj;
  ResolveCredentialMetadata(props, ud, user_proj);
  EXPECT_EQ(user_proj, "login-quota-proj");

  std::remove(login_config_file.c_str());
}

TEST(InteractiveOAuthSignIn, RejectsStateMismatch) {
  ConnProps props{};
  props.auth_mechanism = OauthMechanism::kUserAccount;

  std::thread client_thread;
  auto launcher = [&](std::string const& url) -> odbc_internal::StatusRecord {
    auto q = ParseUrlQueryForTest(url);
    int port = ExtractPortFromRedirectUri(q["redirect_uri"]);
    client_thread = std::thread([port]() {
      SendLoopbackCallbackRequest(port, "/?code=abc&state=tampered_state");
    });
    return odbc_internal::StatusRecord::Ok();
  };

  auto res = RunInteractiveOAuthSignIn(props, Options{}, launcher, nullptr,
                                       std::chrono::seconds(5));
  if (client_thread.joinable()) client_thread.join();
  EXPECT_THAT(res, StatusRecordIs(odbc_internal::SQLStates::k_28000(),
                                  HasSubstr("state mismatch")));
}

TEST(InteractiveOAuthSignIn, CancelsWaitWhenRequested) {
  ConnProps props{};
  props.auth_mechanism = OauthMechanism::kUserAccount;

  std::thread cancel_thread;
  auto launcher = [&](std::string const&) -> odbc_internal::StatusRecord {
    cancel_thread = std::thread([]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      CancelInteractiveOAuthSignIn();
    });
    return odbc_internal::StatusRecord::Ok();
  };

  auto res = RunInteractiveOAuthSignIn(props, Options{}, launcher, nullptr,
                                       std::chrono::seconds(30));
  if (cancel_thread.joinable()) cancel_thread.join();
  EXPECT_THAT(res, StatusRecordIs(odbc_internal::SQLStates::k_HY000(),
                                  HasSubstr("cancelled")));
}

}  // namespace google::cloud::odbc_bq_driver_internal
