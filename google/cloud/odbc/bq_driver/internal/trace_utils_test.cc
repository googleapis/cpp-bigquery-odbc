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

#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;
using google::cloud::odbc_testing_utils::StatusRecordIs;
using ::testing::HasSubstr;

// Common Test Values.
Section const kDriverSection1{{"LogLevel", "1"}, {"LogFile", "/tmp/odbc.log"}};
Section const kDriverSection2{{"LogLevel", "0"}, {"LogFile", "/tmp/odbc.log"}};
Section const kDriverSection3{{"LogFile", "/tmp/odbc.log"}};
Section const kDriverSection4{{"LogLevel", "4"}, {"LogFile", "/tmp/odbc.log"}};
Section const kDriverSection5{{"LogLevel", "1"}};
Section const kDriverSection6{{"LogLevel", ""}};
Section const kDriverSection7{{"LogLevel", "INVALID"}};

Sections const kConfigSections1{{"Driver", kDriverSection1}};
Sections const kConfigSections2{{"Driver", kDriverSection2}};
Sections const kConfigSections3{{"Driver", kDriverSection3}};
Sections const kConfigSections4{{"Driver", kDriverSection4}};
Sections const kConfigSections5{{"Driver", kDriverSection5}};
Sections const kConfigSections6{{"Driver", kDriverSection6}};
Sections const kConfigSections7{{"Driver", kDriverSection7}};

std::shared_ptr<TraceOptions> test_opts_console =
    *(TraceOptions::CreateTraceOptionsConsole(true, 0));

TEST(TraceLoggingFile, TraceOptionsFromConfigTraceEnabled) {
  auto config_sections = std::make_shared<Sections>(kConfigSections1);
  StatusRecordOr<std::shared_ptr<TraceOptions>> test_opts_file =
      TraceOptions::CreateTraceOptionsFile(config_sections);
  ASSERT_STATUS_RECORD_OK(test_opts_file);

  EXPECT_TRUE((*test_opts_file)->logging_enabled);
  EXPECT_TRUE((*test_opts_file)->trace_file.is_open());
  EXPECT_EQ(1, (*test_opts_file)->log_level);

  (*test_opts_file)->trace_file.close();
}

TEST(TraceLoggingFile, TraceOptionsFromConfigTraceDisabled) {
  auto config_sections = std::make_shared<Sections>(kConfigSections2);
  StatusRecordOr<std::shared_ptr<TraceOptions>> test_opts_file =
      TraceOptions::CreateTraceOptionsFile(config_sections);
  ASSERT_STATUS_RECORD_OK(test_opts_file);

  EXPECT_FALSE((*test_opts_file)->logging_enabled);
  EXPECT_TRUE((*test_opts_file)->trace_file.is_open());
  EXPECT_EQ(0, (*test_opts_file)->log_level);
}

TEST(TraceLoggingFile, TraceOptionsFromConfigTraceAbsent) {
  auto config_sections = std::make_shared<Sections>(kConfigSections3);
  StatusRecordOr<std::shared_ptr<TraceOptions>> test_opts_file =
      TraceOptions::CreateTraceOptionsFile(config_sections);
  ASSERT_STATUS_RECORD_OK(test_opts_file);

  EXPECT_FALSE((*test_opts_file)->logging_enabled);
  EXPECT_TRUE((*test_opts_file)->trace_file.is_open());
  EXPECT_EQ(0, (*test_opts_file)->log_level);
}

TEST(TraceLoggingFile, TraceOptionsFromConfigTraceLevel4) {
  auto config_sections = std::make_shared<Sections>(kConfigSections4);
  StatusRecordOr<std::shared_ptr<TraceOptions>> test_opts_file =
      TraceOptions::CreateTraceOptionsFile(config_sections);
  ASSERT_STATUS_RECORD_OK(test_opts_file);

  EXPECT_TRUE((*test_opts_file)->logging_enabled);
  EXPECT_TRUE((*test_opts_file)->trace_file.is_open());
  EXPECT_EQ(4, (*test_opts_file)->log_level);

  (*test_opts_file)->trace_file.close();
}

TEST(TraceLoggingFile, TraceOptionsFromConfigTraceFileAbsent) {
  auto config_sections = std::make_shared<Sections>(kConfigSections5);
  StatusRecordOr<std::shared_ptr<TraceOptions>> test_opts_file =
      TraceOptions::CreateTraceOptionsFile(config_sections);
  ASSERT_STATUS_RECORD_OK(test_opts_file);

  EXPECT_TRUE((*test_opts_file)->logging_enabled);
  EXPECT_TRUE((*test_opts_file)->trace_file.is_open());
  EXPECT_EQ(1, (*test_opts_file)->log_level);
}

TEST(TraceLoggingFile, TraceOptionsEmptyConfigs) {
  std::shared_ptr<Sections> config_sections = nullptr;
  auto opts = TraceOptions::CreateTraceOptionsFile(config_sections);
  EXPECT_THAT(
      opts, StatusRecordIs(SQLStates::k_HY000(), "Invalid ODBC Driver Config"));
}

TEST(TraceLoggingFile, TraceOptionsFromConfigEmptyLogLevel) {
  auto config_sections = std::make_shared<Sections>(kConfigSections6);
  StatusRecordOr<std::shared_ptr<TraceOptions>> test_opts_file =
      TraceOptions::CreateTraceOptionsFile(config_sections);

  EXPECT_FALSE((*test_opts_file)->logging_enabled);
  EXPECT_TRUE((*test_opts_file)->trace_file.is_open());
  EXPECT_EQ(0, (*test_opts_file)->log_level);
}

TEST(TraceLoggingFile, TraceOptionsFromConfigInvalidLogLevel) {
  auto config_sections = std::make_shared<Sections>(kConfigSections7);
  StatusRecordOr<std::shared_ptr<TraceOptions>> test_opts_file =
      TraceOptions::CreateTraceOptionsFile(config_sections);

  EXPECT_FALSE((*test_opts_file)->logging_enabled);
  EXPECT_TRUE((*test_opts_file)->trace_file.is_open());
  EXPECT_EQ(0, (*test_opts_file)->log_level);
}

TEST(TraceLoggingConsole, BasicODBCTypes) {
  std::string fmt1 = FormatSqlSmallInt(1);
  std::string fmt2 = FormatSqlUSmallInt(2);
  std::string fmt3 = FormatSqlInteger(3);
  std::string fmt4 = FormatSqlUInteger(4);

  EXPECT_EQ(
      "TestBasicODBCTypes\t\tSQLSMALLINT, 1\n\t\tSQLUSMALLINT, "
      "2\n\t\tSQLINTEGER, 3\n\t\tSQLUINTEGER, 4\n",
      CollectAndPrintArgs("TestBasicODBCTypes", *test_opts_console, 4,
                          fmt1.c_str(), fmt2.c_str(), fmt3.c_str(),
                          fmt4.c_str()));
}

TEST(TraceLoggingConsole, Handle) {
  std::string expected;
  expected.append("TestHandle\t\tSQL_NULL_HANDLE, 0x0\n\t\t")
      .append("SQL_HANDLE_DBC, handle type=2\n\t\t")
      .append("SQL_HANDLE_DESC, handle type=4\n\t\t")
      .append("SQL_HANDLE_ENV, handle type=1\n\t\t")
      .append("SQL_HANDLE_STMT, handle type=3\n\t\t")
      .append("Unknown Handle Type, handle type=1234\n");

  SQLHANDLE handle = nullptr;

  std::string fmt1 = FormatSqlHandle(handle);
  std::string fmt2 = FormatSqlHandleType(SQL_HANDLE_DBC);
  std::string fmt3 = FormatSqlHandleType(SQL_HANDLE_DESC);
  std::string fmt4 = FormatSqlHandleType(SQL_HANDLE_ENV);
  std::string fmt5 = FormatSqlHandleType(SQL_HANDLE_STMT);
  std::string fmt6 = FormatSqlHandleType(1234);

  EXPECT_EQ(expected,
            CollectAndPrintArgs("TestHandle", *test_opts_console, 6,
                                fmt1.c_str(), fmt2.c_str(), fmt3.c_str(),
                                fmt4.c_str(), fmt5.c_str(), fmt6.c_str()));
}

TEST(TraceLoggingConsole, Pointers) {
  SQLPOINTER p = nullptr;
  SQLPOINTER* pp = nullptr;
  SQLHANDLE* hp = nullptr;
  SQLSMALLINT i1 = 1;
  SQLUSMALLINT i2 = 2;
  SQLINTEGER i3 = 3;
  SQLUINTEGER i4 = 4;
  SQLCHAR* str = (SQLCHAR*)"Hello World";

  std::string fmt1 = FormatSqlPointer(p);
  std::string fmt2 = FormatSqlSmallInt(&i1);
  std::string fmt3 = FormatSqlUSmallInt(&i2);
  std::string fmt4 = FormatSqlInteger(&i3);
  std::string fmt5 = FormatSqlUInteger(&i4);
  std::string fmt6 = FormatSqlChar(str);
  std::string fmt7 = FormatSqlPointer(pp);
  std::string fmt8 = FormatSqlHandle(hp);

  std::string expected;
  expected.append("TestPointers\t\tSQLPOINTER, 0x0\n\t\t")
      .append("SQLSMALLINT *, 1\n\t\tSQLUSMALLINT *, 2\n\t\t")
      .append("SQLINTEGER *, 3\n\t\tSQLUINTEGER *, 4\n\t\t")
      .append(
          "SQLCHAR *, Hello World\n\t\tSQLPOINTER *, 0x0\n\t\tSQLHANDLE *, "
          "0x0\n");

  EXPECT_EQ(expected,
            CollectAndPrintArgs("TestPointers", *test_opts_console, 8,
                                fmt1.c_str(), fmt2.c_str(), fmt3.c_str(),
                                fmt4.c_str(), fmt5.c_str(), fmt6.c_str(),
                                fmt7.c_str(), fmt8.c_str()));
}

TEST(TraceLoggingConsole, Length) {
  std::string fmt1 = FormatSqlLen(10);
  std::string fmt2 = FormatSqlULen(11);
  std::string fmt3 = FormatSqlSetPosiRow(50);

  EXPECT_EQ(
      "TestLength\t\tSQLLEN, 10\n\t\tSQLULEN, 11\n\t\tSQLSETPOSIROW, 50\n",
      CollectAndPrintArgs("TestLength", *test_opts_console, 3, fmt1.c_str(),
                          fmt2.c_str(), fmt3.c_str()));
}

TEST(TraceLoggingConsole, ReturnCodes) {
  std::string fmt1 = FormatSqlReturnCode(1);
  std::string fmt2 = FormatSqlReturn(2);

  EXPECT_EQ("TestRetCodes\t\tRETCODE, 1\n\t\tSQLRETURN, 2\n",
            CollectAndPrintArgs("TestRetCodes", *test_opts_console, 2,
                                fmt1.c_str(), fmt2.c_str()));
}

TEST(TraceLoggingConsole, AdditionalSqlTypes) {
  SQLDATE* d = (SQLDATE*)"1901-01-01";
  SQLTIME* t = (SQLTIME*)"10:30:00";
  SQLTIMESTAMP* tp = (SQLTIMESTAMP*)"1901-01-01 10:30:00";
  SQLVARCHAR* str = (SQLVARCHAR*)"Hello";

  SQLDECIMAL dec = 10;
  SQLNUMERIC n = 11;
  SQLDOUBLE dbl = 1.1;
  SQLFLOAT fl = 2.2;
  SQLREAL r = 3.3f;

  std::string fmt1 = FormatSqlDate(d);
  std::string fmt2 = FormatSqlDecimal(dec);
  std::string fmt3 = FormatSqlNumeric(n);
  std::string fmt4 = FormatSqlDouble(dbl);
  std::string fmt5 = FormatSqlFloat(fl);
  std::string fmt6 = FormatSqlReal(r);
  std::string fmt7 = FormatSqlTime(t);
  std::string fmt8 = FormatSqlTimestamp(tp);
  std::string fmt9 = FormatSqlVarchar(str);
  std::string fmt10 = FormatSqlDecimal(&dec);
  std::string fmt11 = FormatSqlNumeric(&n);
  std::string fmt12 = FormatSqlDouble(&dbl);
  std::string fmt13 = FormatSqlFloat(&fl);
  std::string fmt14 = FormatSqlReal(&r);

  std::string expected;
  expected
      .append(
          "TestAdditionalSqlTypes\t\tSQLDATE *, 1901-01-01\n\t\tSQLDECIMAL, "
          "10\n\t\t")
      .append(
          "SQLNUMERIC, 11\n\t\tSQLDOUBLE, 1.1000\n\t\tSQLFLOAT, 2.2000\n\t\t")
      .append("SQLREAL, 3.30\n\t\tSQLTIME *, 10:30:00\n\t\t")
      .append(
          "SQLTIMESTAMP *, 1901-01-01 10:30:00\n\t\tSQLVARCHAR *, Hello\n\t\t")
      .append(
          "SQLDECIMAL *, 10\n\t\tSQLNUMERIC *, 11\n\t\tSQLDOUBLE *, "
          "1.1000\n\t\t")
      .append("SQLFLOAT *, 2.2000\n\t\tSQLREAL *, 3.30\n");

  EXPECT_EQ(expected,
            CollectAndPrintArgs("TestAdditionalSqlTypes", *test_opts_console,
                                14, fmt1.c_str(), fmt2.c_str(), fmt3.c_str(),
                                fmt4.c_str(), fmt5.c_str(), fmt6.c_str(),
                                fmt7.c_str(), fmt8.c_str(), fmt9.c_str(),
                                fmt10.c_str(), fmt11.c_str(), fmt12.c_str(),
                                fmt13.c_str(), fmt14.c_str()));
}

TEST(TraceLoggingConsole, BasicTypesString) {
  std::string str1 = "Hello1";
  char const* str2 = "Hello2";
  char const str3[] = "Hello3";

  EXPECT_EQ("TestBasicTypesString\t\tHello1\n\t\tHello2\n\t\tHello3\n",
            CollectAndPrintArgs("TestBasicTypesString", *test_opts_console, 3,
                                FormatString(str1).c_str(),
                                FormatCharString(str2).c_str(),
                                FormatCharArray(str3).c_str()));
}

TEST(TraceLoggingConsole, BasicTypesChar) {
  char c1 = 'A';
  unsigned char c2 = 'B';

  EXPECT_EQ(
      "TestBasicTypesChar\t\tA\n\t\tB\n",
      CollectAndPrintArgs("TestBasicTypesChar", *test_opts_console, 2,
                          FormatChar(c1).c_str(), FormatCharU(c2).c_str()));
}

TEST(TraceLoggingConsole, BasicTypesInt) {
  int i1 = 1;
  unsigned int i2 = 2;

  EXPECT_EQ("TestBasicTypesInt\t\t1\n\t\t2\n",
            CollectAndPrintArgs("TestBasicTypesInt", *test_opts_console, 2,
                                FormatInt(i1).c_str(), FormatIntU(i2).c_str()));
}

TEST(TraceLoggingConsole, BasicTypesLong) {
  long l1 = 1L;
  unsigned long l2 = 2L;

  EXPECT_EQ(
      "TestBasicTypesLong\t\t1\n\t\t2\n",
      CollectAndPrintArgs("TestBasicTypesLong", *test_opts_console, 2,
                          FormatLong(l1).c_str(), FormatLongU(l2).c_str()));
}

TEST(TraceLoggingConsole, BasicTypesShort) {
  short s1 = 1;
  unsigned short s2 = 2;

  EXPECT_EQ(
      "TestBasicTypesShort\t\t1\n\t\t2\n",
      CollectAndPrintArgs("TestBasicTypesShort", *test_opts_console, 2,
                          FormatShort(s1).c_str(), FormatShortU(s2).c_str()));
}

TEST(TraceLoggingConsole, BasicTypesDouble) {
  double d = 1.1234;
  EXPECT_EQ("TestBasicTypesDouble\t\t1.1234\n",
            CollectAndPrintArgs("TestBasicTypesDouble", *test_opts_console, 1,
                                FormatDouble(d).c_str()));
}

TEST(TraceLoggingConsole, BasicTypesFloat) {
  float f = 1.12;
  EXPECT_EQ("TestBasicTypesFloat\t\t1.12\n",
            CollectAndPrintArgs("TestBasicTypesFloat", *test_opts_console, 1,
                                FormatFloat(f).c_str()));
}

TEST(TraceLoggingConsole, BasicTypesBool) {
  EXPECT_EQ(
      "TestBasicTypesBool\t\tTRUE\n\t\tFALSE\n",
      CollectAndPrintArgs("TestBasicTypesBool", *test_opts_console, 2,
                          FormatBool(true).c_str(), FormatBool(false).c_str()));
}

TEST(TraceLoggingConsole, ExitInternalTraceEnabled) {
  SQLRETURN ret_code = 1001;

  EXPECT_EQ("TestExit\t\tSQLRETURN, 1001\n",
            ExitInternal("TestExit", ret_code, *test_opts_console));
}

TEST(TraceLoggingConsole, ExitInternalTraceDisabled) {
  SQLRETURN ret_code = 1001;
  test_opts_console->logging_enabled = false;

  EXPECT_EQ("", ExitInternal("TestExit", ret_code, *test_opts_console));
  test_opts_console->logging_enabled = true;
}

TEST(TraceLoggingConsole, TracePrintInternalEmptyString) {
  std::string empty_s;

  EXPECT_EQ("", TracePrintInternal(*test_opts_console, empty_s));
}

TEST(TraceLoggingConsole, TracePrintInternalLoggingDisabled) {
  std::string empty_s;

  test_opts_console->logging_enabled = false;
  EXPECT_EQ("", TracePrintInternal(*test_opts_console, empty_s));
  test_opts_console->logging_enabled = true;
}

TEST(TraceLoggingConsole, TracePrintInternalSuccess) {
  std::string msg = "hello world";

  EXPECT_EQ(msg, TracePrintInternal(*test_opts_console, msg));
}

#if (ODBCVER >= 0x0300)

TEST(TraceLoggingConsole, FormatNumericStructPositive) {
  SQL_NUMERIC_STRUCT n;
  n.precision = 2;
  n.scale = 3;
  n.sign = 1;
  n.val[0] = '1';
  n.val[1] = '2';
  n.val[2] = '3';
  n.val[3] = '4';
  n.val[4] = '5';
  n.val[5] = '6';
  n.val[6] = '7';
  n.val[7] = '\0';

  EXPECT_EQ("\t\tSQL_NUMERIC_STRUCT, precision=2, scale=3, val=1234567 \n",
            FormatNumericStruct(n));
}

TEST(TraceLoggingConsole, FormatNumericStructNegative) {
  SQL_NUMERIC_STRUCT n;
  n.precision = 2;
  n.scale = 3;
  n.sign = 0;
  n.val[0] = '1';
  n.val[1] = '2';
  n.val[2] = '3';
  n.val[3] = '4';
  n.val[4] = '5';
  n.val[5] = '6';
  n.val[6] = '7';
  n.val[7] = '\0';

  EXPECT_EQ("\t\tSQL_NUMERIC_STRUCT, precision=2, scale=3, val=(-)1234567 \n",
            FormatNumericStruct(n));
}

TEST(TraceLoggingConsole, FormatDateStruct) {
  SQL_DATE_STRUCT d;
  d.day = 12;
  d.month = 12;
  d.year = 2023;

  EXPECT_EQ("\t\tSQL_DATE_STRUCT, date(YYYY/MM/DD)=2023/12/12\n",
            FormatDateStruct(d));
}

TEST(TraceLoggingConsole, FormatTimeStruct) {
  SQL_TIME_STRUCT t;
  t.hour = 10;
  t.minute = 11;
  t.second = 12;

  EXPECT_EQ("\t\tSQL_TIME_STRUCT, time(hh:mm:ss)=10:11:12\n",
            FormatTimeStruct(t));
}

TEST(TraceLoggingConsole, FormatTimestampStruct) {
  SQL_TIMESTAMP_STRUCT t;
  t.day = 12;
  t.month = 12;
  t.year = 2023;
  t.hour = 10;
  t.minute = 11;
  t.second = 12;
  t.fraction = 123;

  EXPECT_EQ(
      "\t\tSQL_TIMESTAMP_STRUCT, datetime(YYYY/MM/DD hh:mm:ss.sss)=2023/12/12 "
      "10:11:12.123\n",
      FormatTimestampStruct(t));
}

TEST(TraceLoggingConsole, FormatIntervalStructPositive) {
  SQL_INTERVAL_STRUCT t;
  t.interval_sign = 1;
  t.interval_type = SQL_IS_DAY_TO_SECOND;
  t.intval.day_second = {12, 10, 11, 12, 123};

  std::string exp =
      "\t\tSQL_INTERVAL_STRUCT, interval_type=SQL_IS_DAY_TO_SECOND"
      ", interval_sign=(+), \t\tSQL_YEAR_MONTH_STRUCT, "
      "year_month(YYYY/MM)=12/10\n"
      ", \t\tSQL_DAY_SECOND_STRUCT, day_second(DD hh:mm:ss.ssss)=12 "
      "10:11:12.123\n\n";

  EXPECT_EQ(exp, FormatIntervalStruct(t));
}

TEST(TraceLoggingConsole, FormatIntervalStructNegative) {
  SQL_INTERVAL_STRUCT t;
  t.interval_sign = 0;
  t.interval_type = SQL_IS_DAY_TO_SECOND;
  t.intval.day_second = {12, 10, 11, 12, 123};

  std::string exp =
      "\t\tSQL_INTERVAL_STRUCT, interval_type=SQL_IS_DAY_TO_SECOND"
      ", interval_sign=(-), \t\tSQL_YEAR_MONTH_STRUCT, "
      "year_month(YYYY/MM)=12/10\n"
      ", \t\tSQL_DAY_SECOND_STRUCT, day_second(DD hh:mm:ss.ssss)=12 "
      "10:11:12.123\n\n";

  EXPECT_EQ(exp, FormatIntervalStruct(t));
}
#endif

#ifdef WIN32
TEST(TraceLoggingConsole, WindowHandles) {
  HWND w1 = nullptr;
  SQLHWND w2 = nullptr;

  std::string fmt1 = FormatHWND(w1);
  std::string fmt2 = FormatSqlHWND(w2);

  EXPECT_EQ("TestWindowHandles\t\tHWND, 0x0\n\t\tSQLHWND 0x0\n",
            CollectAndPrintArgs("TestWindowHandles", *test_opts_console, 2,
                                fmt1.c_str(), fmt2.c_str()));
}
#endif /* WIN32 */

}  // namespace google::cloud::odbc_bq_driver_internal
