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
#include <gtest/gtest.h>

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace google {
namespace cloud {
namespace odbc_bq_driver { 

class TraceLoggingConsole : public ::testing::Test {
 protected:
  void SetUp() override
  {
    auto opts = TraceOptions::CreateTraceOptionsConsole(true, 0);
    if (opts.ok()) {
      opts_ = *opts;
    }
  }

  void EnableLogging()
  {
    std::lock_guard<std::mutex>(opts_->m);
    opts_->logging_enabled = true;
  }

  void DisableLogging()
  {
    std::lock_guard<std::mutex>(opts_->m);
    opts_->logging_enabled = false;
  }

  void TearDown() override {}

  std::shared_ptr<TraceOptions> opts_;
};

class TraceLoggingFile : public ::testing::Test {
 protected:
  void SetUp() override
  {
    const Section kOdbcSection{
        {"Trace", "1"},
        {"TraceFile", "/tmp/odbc.log"}};

    const Sections kConfigSections{
        {"ODBC", kOdbcSection}};

    auto config_sections = std::make_shared<Sections>(kConfigSections);
    auto opts = TraceOptions::CreateTraceOptionsFromODBCConfigs(config_sections);
    if (opts.ok()) {
      opts_ = *opts;
    }
  }

  void TearDown() override
  {
    std::lock_guard<std::mutex>(opts_->m);
    if (opts_->trace_file.is_open())
    {
      opts_->trace_file.close();
    }
  }

  std::shared_ptr<TraceOptions> opts_;
};

TEST_F(TraceLoggingFile, TraceOptionsFromConfigSuccess)
{
  EXPECT_TRUE(opts_->logging_enabled);
  EXPECT_TRUE(opts_->trace_file.is_open());
  EXPECT_EQ(1, opts_->log_level);
}

TEST(TraceLogging, TraceOptionsFromConfigEmpty)
{
  std::shared_ptr<Sections> configs = nullptr;
  auto opts = TraceOptions::CreateTraceOptionsFromODBCConfigs(configs);
  ASSERT_FALSE(opts.status().ok());
  
  EXPECT_EQ(opts.status().code(), StatusCode::kInvalidArgument);
  EXPECT_EQ(opts.status().message(), "Invalid ODBC Config");
}

TEST_F(TraceLoggingConsole, BasicODBCTypes)
{
  std::string fmt1 = FormatSqlSmallInt(1);
  std::string fmt2 = FormatSqlUSmallInt(2);
  std::string fmt3 = FormatSqlInteger(3);
  std::string fmt4 = FormatSqlUInteger(4);

  EXPECT_EQ("TestBasicODBCTypes\t\tSQLSMALLINT, 1\n\t\tSQLUSMALLINT, 2\n\t\tSQLINTEGER, 3\n\t\tSQLUINTEGER, 4\n",
            CollectAndPrintArgs("TestBasicODBCTypes", *opts_,
                                4, fmt1.c_str(), fmt2.c_str(), fmt3.c_str(), fmt4.c_str()));
}

TEST_F(TraceLoggingConsole, Handle)
{
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

  EXPECT_EQ(expected, CollectAndPrintArgs("TestHandle", *opts_,
                                          6, fmt1.c_str(), fmt2.c_str(), fmt3.c_str(),
                                          fmt4.c_str(), fmt5.c_str(), fmt6.c_str()));
}

TEST_F(TraceLoggingConsole, Pointers)
{
  SQLPOINTER p = nullptr;
  SQLPOINTER *pp = nullptr;
  SQLHANDLE *hp = nullptr;
  SQLSMALLINT i1 = 1;
  SQLUSMALLINT i2 = 2;
  SQLINTEGER i3 = 3;
  SQLUINTEGER i4 = 4;
  SQLCHAR *str = (SQLCHAR *)"Hello World";

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
      .append("SQLCHAR *, Hello World\n\t\tSQLPOINTER *, 0x0\n\t\tSQLHANDLE *, 0x0\n");

  EXPECT_EQ(expected, CollectAndPrintArgs("TestPointers", *opts_,
                                          8, fmt1.c_str(), fmt2.c_str(), fmt3.c_str(), fmt4.c_str(),
                                          fmt5.c_str(), fmt6.c_str(), fmt7.c_str(), fmt8.c_str()));
}

TEST_F(TraceLoggingConsole, Length)
{
  std::string fmt1 = FormatSqlLen(10);
  std::string fmt2 = FormatSqlULen(11);
  std::string fmt3 = FormatSqlSetPosiRow(50);

  EXPECT_EQ("TestLength\t\tSQLLEN, 10\n\t\tSQLULEN, 11\n\t\tSQLSETPOSIROW, 50\n",
            CollectAndPrintArgs("TestLength", *opts_, 3, fmt1.c_str(), fmt2.c_str(), fmt3.c_str()));
}

TEST_F(TraceLoggingConsole, ReturnCodes)
{
  std::string fmt1 = FormatSqlReturnCode(1);
  std::string fmt2 = FormatSqlReturn(2);

  EXPECT_EQ("TestRetCodes\t\tRETCODE, 1\n\t\tSQLRETURN, 2\n",
            CollectAndPrintArgs("TestRetCodes", *opts_, 2, fmt1.c_str(), fmt2.c_str()));
}

TEST_F(TraceLoggingConsole, AdditionalSqlTypes)
{
  SQLDATE *d = (SQLDATE *)"1901-01-01";
  SQLTIME *t = (SQLTIME *)"10:30:00";
  SQLTIMESTAMP *tp = (SQLTIMESTAMP *)"1901-01-01 10:30:00";
  SQLVARCHAR *str = (SQLVARCHAR *)"Hello";

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
  expected.append("TestAdditionalSqlTypes\t\tSQLDATE *, 1901-01-01\n\t\tSQLDECIMAL, 10\n\t\t")
      .append("SQLNUMERIC, 11\n\t\tSQLDOUBLE, 1.1000\n\t\tSQLFLOAT, 2.2000\n\t\t")
      .append("SQLREAL, 3.30\n\t\tSQLTIME *, 10:30:00\n\t\t")
      .append("SQLTIMESTAMP *, 1901-01-01 10:30:00\n\t\tSQLVARCHAR *, Hello\n\t\t")
      .append("SQLDECIMAL *, 10\n\t\tSQLNUMERIC *, 11\n\t\tSQLDOUBLE *, 1.1000\n\t\t")
      .append("SQLFLOAT *, 2.2000\n\t\tSQLREAL *, 3.30\n");

  EXPECT_EQ(expected,
            CollectAndPrintArgs("TestAdditionalSqlTypes", *opts_, 14,
                                fmt1.c_str(), fmt2.c_str(), fmt3.c_str(), fmt4.c_str(),
                                fmt5.c_str(), fmt6.c_str(), fmt7.c_str(), fmt8.c_str(),
                                fmt9.c_str(), fmt10.c_str(), fmt11.c_str(), fmt12.c_str(),
                                fmt13.c_str(), fmt14.c_str()));
}

TEST_F(TraceLoggingConsole, BasicTypesString)
{
  std::string str1 = "Hello1";
  const char *str2 = "Hello2";
  const char str3[] = "Hello3";

  EXPECT_EQ("TestBasicTypesString\t\tHello1\n\t\tHello2\n\t\tHello3\n",
            CollectAndPrintArgs("TestBasicTypesString", *opts_, 3,
                                FormatString(str1).c_str(), FormatCharString(str2).c_str(),
                                FormatCharArray(str3).c_str()));
}

TEST_F(TraceLoggingConsole, BasicTypesChar)
{
  char c1 = 'A';
  unsigned char c2 = 'B';

  EXPECT_EQ("TestBasicTypesChar\t\tA\n\t\tB\n",
            CollectAndPrintArgs("TestBasicTypesChar", *opts_, 2,
                                FormatChar(c1).c_str(), FormatCharU(c2).c_str()));
}

TEST_F(TraceLoggingConsole, BasicTypesInt)
{
  int i1 = 1;
  unsigned int i2 = 2;

  EXPECT_EQ("TestBasicTypesInt\t\t1\n\t\t2\n",
            CollectAndPrintArgs("TestBasicTypesInt", *opts_, 2,
                                FormatInt(i1).c_str(), FormatIntU(i2).c_str()));
}

TEST_F(TraceLoggingConsole, BasicTypesLong)
{
  long l1 = 1L;
  unsigned long l2 = 2L;

  EXPECT_EQ("TestBasicTypesLong\t\t1\n\t\t2\n",
            CollectAndPrintArgs("TestBasicTypesLong", *opts_, 2,
                                FormatLong(l1).c_str(), FormatLongU(l2).c_str()));
}

TEST_F(TraceLoggingConsole, BasicTypesShort)
{
  short s1 = 1;
  unsigned short s2 = 2;

  EXPECT_EQ("TestBasicTypesShort\t\t1\n\t\t2\n",
            CollectAndPrintArgs("TestBasicTypesShort", *opts_, 2,
                                FormatShort(s1).c_str(), FormatShortU(s2).c_str()));
}

TEST_F(TraceLoggingConsole, BasicTypesDouble)
{
  double d = 1.1234;
  EXPECT_EQ("TestBasicTypesDouble\t\t1.1234\n",
            CollectAndPrintArgs("TestBasicTypesDouble", *opts_, 1, FormatDouble(d).c_str()));
}

TEST_F(TraceLoggingConsole, BasicTypesFloat)
{
  float f = 1.12;
  EXPECT_EQ("TestBasicTypesFloat\t\t1.12\n",
            CollectAndPrintArgs("TestBasicTypesFloat", *opts_, 1, FormatFloat(f).c_str()));
}

TEST_F(TraceLoggingConsole, BasicTypesBool)
{
  EXPECT_EQ("TestBasicTypesBool\t\tTRUE\n\t\tFALSE\n",
            CollectAndPrintArgs(
              "TestBasicTypesBool", *opts_, 2, FormatBool(true).c_str(), FormatBool(false).c_str()));
}

TEST_F(TraceLoggingConsole, ExitInternalTraceEnabled)
{
  SQLRETURN ret_code = 1001;

  EXPECT_EQ("TestExit\t\tSQLRETURN, 1001\n", ExitInternal("TestExit", ret_code, *opts_));
}

TEST_F(TraceLoggingConsole, ExitInternalTraceDisabled)
{
  SQLRETURN ret_code = 1001;
  DisableLogging();

  EXPECT_EQ("", ExitInternal("TestExit", ret_code, *opts_));
  EnableLogging();
}

#if (ODBCVER >= 0x0300)

TEST_F(TraceLoggingConsole, FormatNumericStructPositive)
{
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

TEST_F(TraceLoggingConsole, FormatNumericStructNegative)
{
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

  EXPECT_EQ("\t\tSQL_NUMERIC_STRUCT, precision=2, scale=3, val=(-)1234567 \n", FormatNumericStruct(n));
}

TEST_F(TraceLoggingConsole, FormatDateStruct)
{
  SQL_DATE_STRUCT d;
  d.day = 12;
  d.month = 12;
  d.year = 2023;

  EXPECT_EQ("\t\tSQL_DATE_STRUCT, date(YYYY/MM/DD)=2023/12/12\n", FormatDateStruct(d));
}

TEST_F(TraceLoggingConsole, FormatTimeStruct)
{
  SQL_TIME_STRUCT t;
  t.hour = 10;
  t.minute = 11;
  t.second = 12;

  EXPECT_EQ("\t\tSQL_TIME_STRUCT, time(hh:mm:ss)=10:11:12\n", FormatTimeStruct(t));
}

TEST_F(TraceLoggingConsole, FormatTimestampStruct)
{
  SQL_TIMESTAMP_STRUCT t;
  t.day = 12;
  t.month = 12;
  t.year = 2023;
  t.hour = 10;
  t.minute = 11;
  t.second = 12;
  t.fraction = 123;

  EXPECT_EQ("\t\tSQL_TIMESTAMP_STRUCT, datetime(YYYY/MM/DD hh:mm:ss.sss)=2023/12/12 10:11:12.123\n",
            FormatTimestampStruct(t));
}

TEST_F(TraceLoggingConsole, FormatIntervalStructPositve)
{
  SQL_INTERVAL_STRUCT t;
  t.interval_sign = 1;
  t.interval_type = SQL_IS_DAY_TO_SECOND;
  t.intval.day_second = {12, 10, 11, 12, 123};

  std::string exp = "\t\tSQL_INTERVAL_STRUCT, interval_type=SQL_IS_DAY_TO_SECOND"
                    ", interval_sign=(+), \t\tSQL_YEAR_MONTH_STRUCT, year_month(YYYY/MM)=12/10\n"
                    ", \t\tSQL_DAY_SECOND_STRUCT, day_second(DD hh:mm:ss.ssss)=12 10:11:12.123\n\n";

  EXPECT_EQ(exp, FormatIntervalStruct(t));
}

TEST_F(TraceLoggingConsole, FormatIntervalStructNegative)
{
  SQL_INTERVAL_STRUCT t;
  t.interval_sign = 0;
  t.interval_type = SQL_IS_DAY_TO_SECOND;
  t.intval.day_second = {12, 10, 11, 12, 123};

  std::string exp = "\t\tSQL_INTERVAL_STRUCT, interval_type=SQL_IS_DAY_TO_SECOND"
                    ", interval_sign=(-), \t\tSQL_YEAR_MONTH_STRUCT, year_month(YYYY/MM)=12/10\n"
                    ", \t\tSQL_DAY_SECOND_STRUCT, day_second(DD hh:mm:ss.ssss)=12 10:11:12.123\n\n";

  EXPECT_EQ(exp, FormatIntervalStruct(t));
}
#endif

#ifdef WIN32
TEST_F(TraceLoggingConsole, WindowHandles)
{
  HWND w1 = nullptr;
  SQLHWND w2 = nullptr;

  std::string fmt1 = FormatHWND(w1);
  std::string fmt2 = FormatSqlHWND(w2);

  EXPECT_EQ("TestWindowHandles\t\tHWND, 0x0\n\t\tSQLHWND 0x0\n",
            CollectAndPrintArgs("TestWindowHandles", *opts_, 2, fmt1.c_str(), fmt2.c_str()));
}
#endif  /* WIN32 */

}  // namespace odbc_bq_driver
}  // namespace cloud
}  // namespace google
// NOLINTEND(modernize-concat-nested-namespaces)
