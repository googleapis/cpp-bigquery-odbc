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

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;

constexpr int kCharBufSize1 = 1024;
constexpr int kCharBufSize2 = 256;

// Initialize the Singleton instance.
std::shared_ptr<TraceOptions> TraceOptions::options_console_ = nullptr;
std::shared_ptr<TraceOptions> TraceOptions::options_file_ = nullptr;
std::mutex TraceOptions::mu_;

StatusRecordOr<std::shared_ptr<TraceOptions>>
TraceOptions::GetTraceOption() {
  if (options_file_!=nullptr) {
    return options_file_;
  }
  else if(options_console_!=nullptr){
    return options_console_;
  }
}

StatusRecordOr<std::shared_ptr<TraceOptions>>
TraceOptions::CreateTraceOptionsConsole(bool logging_enabled, int log_level) {
  std::lock_guard<std::mutex> lk(mu_);
  if (options_console_ == nullptr) {
    // Cannot use std::make_shared because constructor is protected.
    options_console_ = std::shared_ptr<TraceOptions>(new TraceOptions());
  }

  options_console_->log_level = log_level;
  options_console_->logging_enabled = logging_enabled;

  return options_console_;
}

StatusRecordOr<std::shared_ptr<TraceOptions>>
TraceOptions::CreateTraceOptionsFile(std::string const& file_path) {
  auto configs = ParseConfig(file_path);
  if (!configs) {
    return configs.GetStatusRecord();
  }

  return CreateTraceOptionsFile(*configs);
}

StatusRecordOr<std::shared_ptr<TraceOptions>>
TraceOptions::CreateTraceOptionsFile(
    std::shared_ptr<Sections> const& config_sections) {
  if (!config_sections) {
    return StatusRecord{SQLStates::k_HY000(), "Invalid ODBC Driver Config"};
  }

  Section trace_sections;
  auto const odbc_section = config_sections->find("Driver");
  if (odbc_section != config_sections->end()) {
    trace_sections = odbc_section->second;
  }

  std::string log_file;
  int log_level = 0;
  bool logging_enabled = false;
  for (auto const& s : trace_sections) {
    if (s.first == "LogLevel" && !s.second.empty()) {
      log_level = std::strtol(s.second.c_str(), nullptr, 10);
      if (log_level > 0) {
        logging_enabled = true;
      }
    } else if (s.first == "LogFile") {
      log_file = s.second;
    }
  }

  std::lock_guard<std::mutex> lk(mu_);
  if (options_file_ == nullptr) {
    // Cannot use std::make_shared because constructor is protected.
    options_file_ = std::shared_ptr<TraceOptions>(new TraceOptions());
  }

  options_file_->log_level = log_level;
  options_file_->logging_enabled = logging_enabled;

  if (logging_enabled && !log_file.empty()) {
    // We are not creating a default log file. If log file is not specified
    // then we will log to console.
    if (!options_file_->trace_file.is_open()) {
      options_file_->trace_file.open(log_file,
                                     std::ofstream::out | std::ofstream::app);
    }
    if (!options_file_->trace_file.is_open()) {
      std::string msg = "Cannot open log file: ";
      msg.append(log_file);
      return StatusRecord{SQLStates::k_HY000(), msg};
    }
  }

  return options_file_;
}

int TracePrintInternalStdOut(TraceOptions& opts, std::string const& s) {
  if (!opts.logging_enabled || s.empty()) {
    return -1;
  }
  std::lock_guard<std::mutex> lk(
      opts.m);  // Releases the mutex when going out of scope.
  std::cout << s << std::endl;
  return 0;
}

int TracePrintInternalFile(TraceOptions& opts, std::string const& s) {
  if (!opts.logging_enabled || s.empty()) {
    return -1;
  }
  std::lock_guard<std::mutex> lk(
      opts.m);  // Releases the mutex when going out of scope.
  if (!opts.trace_file.is_open()) {
    return -1;
  }
  opts.trace_file << s << std::endl;

  return 0;
}

std::string TracePrintInternal(TraceOptions& opts, std::string const& s) {
  if (!opts.logging_enabled || s.empty()) {
    return "";
  }

  int ret = 0;
  if (opts.trace_file.is_open()) {
    ret = TracePrintInternalFile(opts, s);
  } else {
    ret = TracePrintInternalStdOut(opts, s);
  }
  if (ret < 0) {
    return "";
  }

  return s;
}

std::string CollectArgs(va_list src_args, int num_args) {
  std::string trace_str;
  va_list dest_args;
  va_copy(dest_args, src_args);
  for (int i = 0; i < num_args; i++) {
    std::string s = va_arg(dest_args, char const*);
    trace_str.append(s);
  }
  va_end(dest_args);  // src_args needs to be ended by the caller.
  return trace_str;
}

std::string CollectAndPrintArgs(std::string const& func_name,
                                TraceOptions& opts, int num_args, ...) {
  std::string trace_str;
  trace_str.append(func_name);

  if (num_args > 0) {
    va_list args_list;
    va_start(args_list, num_args);
    trace_str.append(CollectArgs(args_list, num_args));
    va_end(args_list);

    int ret = TracePrintInternalStdOut(opts, trace_str);
    if (ret < 0) {
      return "";
    }
  }
  return trace_str;
}

std::string CollectAndPrintArgsFile(std::string const& func_name,
                                    TraceOptions& opts, int num_args, ...) {
  std::string trace_str;
  trace_str.append(func_name);

  if (num_args > 0) {
    va_list args_list;
    va_start(args_list, num_args);
    trace_str.append(CollectArgs(args_list, num_args));
    va_end(args_list);

    int ret = TracePrintInternalFile(opts, trace_str);
    if (ret < 0) {
      return "";
    }
  }
  return trace_str;
}

std::string FormatSqlSmallInt(SQLSMALLINT i) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, %hi\n", "SQLSMALLINT", i);
  return buf;
}

std::string FormatSqlUSmallInt(SQLUSMALLINT i) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, %hu\n", "SQLUSMALLINT", i);
  return buf;
}

std::string FormatSqlInteger(SQLINTEGER i) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, %d\n", "SQLINTEGER", i);
  return buf;
}

std::string FormatSqlUInteger(SQLUINTEGER i) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, %i\n", "SQLUINTEGER", i);
  return buf;
}

std::string FormatSqlHandleType(SQLSMALLINT type) {
  char buf[kCharBufSize1];
  switch (type) {
    case SQL_HANDLE_ENV: {
      sprintf(buf, "\t\t%-s, handle type=%hi\n", "SQL_HANDLE_ENV", type);
      break;
    }
    case SQL_HANDLE_DBC: {
      sprintf(buf, "\t\t%-s, handle type=%hi\n", "SQL_HANDLE_DBC", type);
      break;
    }
    case SQL_HANDLE_DESC: {
      sprintf(buf, "\t\t%-s, handle type=%hi\n", "SQL_HANDLE_DESC", type);
      break;
    }
    case SQL_HANDLE_STMT: {
      sprintf(buf, "\t\t%-s, handle type=%hi\n", "SQL_HANDLE_STMT", type);
      break;
    }
    default: {
      sprintf(buf, "\t\t%-s, handle type=%hi\n", "Unknown Handle Type", type);
    }
  }
  return buf;
}

std::string FormatSqlHandle(SQLHANDLE handle) {
  char buf[kCharBufSize1];
  if (!handle) {
    sprintf(buf, "\t\t%-s, 0x0\n", "SQL_NULL_HANDLE");
  } else {
    sprintf(buf, "\t\t%-s, %p\n", "SQL_HANDLE", handle);
  }
  return buf;
}

std::string FormatSqlPointer(SQLPOINTER p) {
  char buf[kCharBufSize1];
  if (!p) {
    sprintf(buf, "\t\t%-s, 0x0\n", "SQLPOINTER");
  } else {
    sprintf(buf, "\t\t%-s, %p\n", "SQLPOINTER", p);
  }
  return buf;
}

std::string FormatSqlSmallInt(const SQLSMALLINT* p) {
  char buf[kCharBufSize1];
  if (!p)
    sprintf(buf, "\t\t%-s *, 0x0\n", "SQLSMALLINT");
  else
    sprintf(buf, "\t\t%-s *, %hi\n", "SQLSMALLINT", *p);
  return buf;
}

std::string FormatSqlUSmallInt(const SQLUSMALLINT* p) {
  char buf[kCharBufSize1];
  if (!p)
    sprintf(buf, "\t\t%-s *,  0x0\n", "SQLUSMALLINT");
  else
    sprintf(buf, "\t\t%-s *, %hu\n", "SQLUSMALLINT", *p);
  return buf;
}

std::string FormatSqlInteger(const SQLINTEGER* p) {
  char buf[kCharBufSize1];
  if (!p)
    sprintf(buf, "\t\t%-s *, 0x0\n", "SQLINTEGER");
  else
    sprintf(buf, "\t\t%-s *, %d\n", "SQLINTEGER", *p);
  return buf;
}

std::string FormatSqlUInteger(const SQLUINTEGER* p) {
  char buf[kCharBufSize1];
  if (!p)
    sprintf(buf, "\t\t%-s *, 0x0\n", "SQLUINTEGER");
  else
    sprintf(buf, "\t\t%-s *, %i\n", "SQLUINTEGER", *p);
  return buf;
}

std::string FormatSqlChar(const SQLCHAR* p) {
  char buf[kCharBufSize1];

  if (!p)
    sprintf(buf, "\t\t%-s *, 0x0\n", "SQLCHAR");
  else
    sprintf(buf, "\t\t%-s *, %s\n", "SQLCHAR", p);
  return buf;
}

std::string FormatSqlPointer(const SQLPOINTER* p) {
  char buf[kCharBufSize1];
  if (!p)
    sprintf(buf, "\t\t%-s *, 0x0\n", "SQLPOINTER");
  else
    sprintf(buf, "\t\t%-s *, %p\n", "SQLPOINTER", p);
  return buf;
}

std::string FormatSqlHandle(const SQLHANDLE* p) {
  char buf[kCharBufSize1];
  if (!p)
    sprintf(buf, "\t\t%-s *, 0x0\n", "SQLHANDLE");
  else
    sprintf(buf, "\t\t%-s *, %p\n", "SQLHANDLE", p);
  return buf;
}

#if (ODBCVER >= 0x0300)
std::string FormatSqlDate(const SQLDATE* d) {
  char buf[kCharBufSize1];
  if (!d)
    sprintf(buf, "\t\t%-s *, 0x0\n", "SQLDATE");
  else
    sprintf(buf, "\t\t%-s *, %s\n", "SQLDATE", d);

  return buf;
}

std::string FormatSqlDecimal(SQLDECIMAL d) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, %d\n", "SQLDECIMAL", d);
  return buf;
}

std::string FormatSqlDecimal(const SQLDECIMAL* d) {
  char buf[kCharBufSize1];
  if (!d)
    sprintf(buf, "\t\t%-s *, 0x0\n", "SQLDECIMAL");
  else
    sprintf(buf, "\t\t%-s *, %d\n", "SQLDECIMAL", *d);
  return buf;
}

std::string FormatSqlNumeric(SQLNUMERIC n) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, %d\n", "SQLNUMERIC", n);
  return buf;
}

std::string FormatSqlNumeric(const SQLNUMERIC* n) {
  char buf[kCharBufSize1];
  if (!n)
    sprintf(buf, "\t\t%-s *, 0x0\n", "SQLNUMERIC");
  else
    sprintf(buf, "\t\t%-s *, %d\n", "SQLNUMERIC", *n);
  return buf;
}

std::string FormatSqlDouble(SQLDOUBLE d) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, %.4lf\n", "SQLDOUBLE", d);
  return buf;
}

std::string FormatSqlDouble(const SQLDOUBLE* d) {
  char buf[kCharBufSize1];
  if (!d)
    sprintf(buf, "\t\t%-s *, 0x0\n", "SQLDOUBLE");
  else
    sprintf(buf, "\t\t%-s *, %.4lf\n", "SQLDOUBLE", *d);
  return buf;
}

std::string FormatSqlFloat(SQLFLOAT f) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, %.4lf\n", "SQLFLOAT", f);
  return buf;
}

std::string FormatSqlFloat(const SQLFLOAT* f) {
  char buf[kCharBufSize1];
  if (!f)
    sprintf(buf, "\t\t%-s *, 0x0\n", "SQLFLOAT");
  else
    sprintf(buf, "\t\t%-s *, %.4lf\n", "SQLFLOAT", *f);
  return buf;
}

std::string FormatSqlReal(SQLREAL r) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, %.2f\n", "SQLREAL", r);
  return buf;
}

std::string FormatSqlReal(const SQLREAL* r) {
  char buf[kCharBufSize1];
  if (!r)
    sprintf(buf, "\t\t%-s *, 0x0\n", "SQLREAL");
  else
    sprintf(buf, "\t\t%-s *, %.2f\n", "SQLREAL", *r);
  return buf;
}

std::string FormatSqlTime(const SQLTIME* t) {
  char buf[kCharBufSize1];
  if (!t)
    sprintf(buf, "\t\t%-s *, 0x0\n", "SQLTIME");
  else
    sprintf(buf, "\t\t%-s *, %s\n", "SQLTIME", t);

  return buf;
}

std::string FormatSqlTimestamp(const SQLTIMESTAMP* tp) {
  char buf[kCharBufSize1];
  if (!tp)
    sprintf(buf, "\t\t%-s *, 0x0\n", "SQLTIMESTAMP");
  else
    sprintf(buf, "\t\t%-s *, %s\n", "SQLTIMESTAMP", tp);

  return buf;
}

std::string FormatSqlVarchar(const SQLVARCHAR* s) {
  char buf[kCharBufSize1];
  if (!s)
    sprintf(buf, "\t\t%-s *, 0x0\n", "SQLVARCHAR");
  else
    sprintf(buf, "\t\t%-s *, %s\n", "SQLVARCHAR", s);

  return buf;
}
#endif /* ODBCVER >= 0x0300 */

std::string FormatSqlLen(SQLLEN l) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, %ld\n", "SQLLEN", l);
  return buf;
}

std::string FormatSqlULen(SQLULEN l) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, %lu\n", "SQLULEN", l);
  return buf;
}

std::string FormatSqlSetPosiRow(SQLSETPOSIROW rp) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, %hu\n", "SQLSETPOSIROW", rp);
  return buf;
}

std::string FormatSqlReturnCode(RETCODE ret) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, %hi\n", "RETCODE", ret);
  return buf;
}

std::string FormatSqlReturn(SQLRETURN ret) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, %hi\n", "SQLRETURN", ret);
  return buf;
}

std::string FormatSqlLen(const SQLLEN* l) {
  char buf[kCharBufSize1];
  if (!l)
    sprintf(buf, "\t\t%-s *, 0x0\n", "SQLLEN");
  else
    sprintf(buf, "\t\t%-s *, %ld\n", "SQLLEN", *l);
  return buf;
}

std::string FormatSqlULen(const SQLULEN* l) {
  char buf[kCharBufSize1];
  if (!l)
    sprintf(buf, "\t\t%-s *, 0x0\n", "SQLULEN");
  else
    sprintf(buf, "\t\t%-s *, %lu\n", "SQLULEN", *l);
  return buf;
}

std::string FormatString(std::string const& str) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%s\n", str.c_str());
  return buf;
}

std::string FormatCharString(char const* str) {
  char buf[kCharBufSize1];
  if (!str)
    sprintf(buf, "\t\t 0x0 null string\n");
  else
    sprintf(buf, "\t\t%s\n", str);
  return buf;
}

std::string FormatCharArray(char const str[]) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%s\n", str);
  return buf;
}

std::string FormatChar(char c) {
  char buf[kCharBufSize2];
  sprintf(buf, "\t\t%c\n", c);
  return buf;
}

std::string FormatCharU(unsigned char c) {
  char buf[kCharBufSize2];
  sprintf(buf, "\t\t%c\n", c);
  return buf;
}

std::string FormatInt(int d) {
  char buf[kCharBufSize2];
  sprintf(buf, "\t\t%d\n", d);
  return buf;
}

std::string FormatIntU(unsigned int d) {
  char buf[kCharBufSize2];
  sprintf(buf, "\t\t%i\n", d);
  return buf;
}

std::string FormatLong(std::int64_t d) {
  char buf[kCharBufSize2];
  sprintf(buf, "\t\t%ld\n", d);
  return buf;
}

std::string FormatLongU(std::uint64_t d) {
  char buf[kCharBufSize2];
  sprintf(buf, "\t\t%li\n", d);
  return buf;
}

std::string FormatShort(std::int16_t d) {
  char buf[kCharBufSize2];
  sprintf(buf, "\t\t%hi\n", d);
  return buf;
}

std::string FormatShortU(std::uint16_t d) {
  char buf[kCharBufSize2];
  sprintf(buf, "\t\t%hu\n", d);
  return buf;
}

std::string FormatDouble(double d) {
  char buf[kCharBufSize2];
  sprintf(buf, "\t\t%.4lf\n", d);
  return buf;
}

std::string FormatFloat(float d) {
  char buf[kCharBufSize2];
  sprintf(buf, "\t\t%.2f\n", d);
  return buf;
}

std::string FormatPointer(void* p) {
  char buf[kCharBufSize2];
  if (!p)
    sprintf(buf, "\t\t0x0 null pointer\n");
  else
    sprintf(buf, "\t\t%p\n", p);
  return buf;
}

std::string FormatBool(bool b) {
  char buf[kCharBufSize2];
  if (b)
    sprintf(buf, "\t\t%s\n", "TRUE");
  else
    sprintf(buf, "\t\t%s\n", "FALSE");
  return buf;
}

char const* ToCStr(std::string const& str) { return str.c_str(); }

std::string ExitInternal(std::string const& func_name, SQLRETURN ret_code,
                         TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.trace_file.is_open()) {
      return CollectAndPrintArgsFile(func_name, opts, 1,
                                     ToCStr(FormatSqlReturn(ret_code)));
    }
    return CollectAndPrintArgs(func_name, opts, 1,
                               ToCStr(FormatSqlReturn(ret_code)));
  }
  return "";
}

#if (ODBCVER >= 0x0300)
std::string FormatNumericStruct(SQL_NUMERIC_STRUCT n) {
  char buf[kCharBufSize1];
  if (!n.sign)
    sprintf(buf, "\t\t%-s, precision=%d, scale=%d, val=(-)%s \n",
            "SQL_NUMERIC_STRUCT", n.precision, n.scale, n.val);
  else
    sprintf(buf, "\t\t%-s, precision=%d, scale=%d, val=%s \n",
            "SQL_NUMERIC_STRUCT", n.precision, n.scale, n.val);
  return buf;
}

std::string FormatDateStruct(SQL_DATE_STRUCT d) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%s, date(YYYY/MM/DD)=%hi/%hu/%hu\n", "SQL_DATE_STRUCT",
          d.year, d.month, d.day);
  return buf;
}

std::string FormatTimeStruct(SQL_TIME_STRUCT t) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%s, time(hh:mm:ss)=%hu:%hu:%hu\n", "SQL_TIME_STRUCT",
          t.hour, t.minute, t.second);
  return buf;
}

std::string FormatTimestampStruct(SQL_TIMESTAMP_STRUCT ts) {
  char buf[kCharBufSize1];
  sprintf(
      buf,
      "\t\t%s, datetime(YYYY/MM/DD hh:mm:ss.sss)=%hu/%hu/%hu %hu:%hu:%hu.%u\n",
      "SQL_TIMESTAMP_STRUCT", ts.year, ts.month, ts.day, ts.hour, ts.minute,
      ts.second, ts.fraction);
  return buf;
}

std::string GetIntervalType(SQLINTERVAL type) {
  switch (type) {
    case SQL_IS_YEAR:
      return "SQL_IS_YEAR";
    case SQL_IS_MONTH:
      return "SQL_IS_MONTH";
    case SQL_IS_DAY:
      return "SQL_IS_DAY";
    case SQL_IS_HOUR:
      return "SQL_IS_HOUR";
    case SQL_IS_MINUTE:
      return "SQL_IS_MINUTE";
    case SQL_IS_SECOND:
      return "SQL_IS_SECOND";
    case SQL_IS_YEAR_TO_MONTH:
      return "SQL_IS_YEAR_TO_MONTH";
    case SQL_IS_DAY_TO_HOUR:
      return "SQL_IS_DAY_TO_HOUR";
    case SQL_IS_DAY_TO_MINUTE:
      return "SQL_IS_DAY_TO_MINUTE";
    case SQL_IS_DAY_TO_SECOND:
      return "SQL_IS_DAY_TO_SECOND";
    case SQL_IS_HOUR_TO_MINUTE:
      return "SQL_IS_HOUR_TO_MINUTE";
    case SQL_IS_HOUR_TO_SECOND:
      return "SQL_IS_HOUR_TO_SECOND";
    case SQL_IS_MINUTE_TO_SECOND:
      return "SQL_IS_MINUTE_TO_SECOND";
  }

  return "";
}

std::string FormatIntervalYearMonthStruct(SQL_YEAR_MONTH_STRUCT ym) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, year_month(YYYY/MM)=%i/%i\n", "SQL_YEAR_MONTH_STRUCT",
          ym.year, ym.month);
  return buf;
}

std::string FormatIntervalDaySecondStruct(SQL_DAY_SECOND_STRUCT ds) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, day_second(DD hh:mm:ss.ssss)=%i %i:%i:%i.%i\n",
          "SQL_DAY_SECOND_STRUCT", ds.day, ds.hour, ds.minute, ds.second,
          ds.fraction);
  return buf;
}

std::string FormatIntervalStruct(SQL_INTERVAL_STRUCT i) {
  char buf[kCharBufSize1];
  if (i.interval_sign) {
    sprintf(buf, "\t\t%s, interval_type=%s, interval_sign=(+), %s, %s\n",
            "SQL_INTERVAL_STRUCT", ToCStr(GetIntervalType(i.interval_type)),
            ToCStr(FormatIntervalYearMonthStruct(i.intval.year_month)),
            ToCStr(FormatIntervalDaySecondStruct(i.intval.day_second)));
  } else {
    sprintf(buf, "\t\t%s, interval_type=%s, interval_sign=(-), %s, %s\n",
            "SQL_INTERVAL_STRUCT", ToCStr(GetIntervalType(i.interval_type)),
            ToCStr(FormatIntervalYearMonthStruct(i.intval.year_month)),
            ToCStr(FormatIntervalDaySecondStruct(i.intval.day_second)));
  }
  return buf;
}
#endif /* ODBCVER >= 0x0300 */

#ifdef WIN32
std::string FormatHWND(HWND handle) {
  char buf[kCharBufSize1];
  if (!handle)
    sprintf(buf, "\t\t%-s, 0x0\n", "HWND");
  else
    sprintf(buf, "\t\t%-s, %p\n", "HWND", handle);
  return buf;
}

std::string FormatSqlHWND(SQLHWND handle) {
  char buf[kCharBufSize1];
  if (!handle)
    sprintf(buf, "\t\t%-s, 0x0\n", "SQLHWND");
  else
    sprintf(buf, "\t\t%-s, %p\n", "SQLHWND", handle);
  return buf;
}
#endif /* WIN32 */

}  // namespace google::cloud::odbc_bq_driver_internal
