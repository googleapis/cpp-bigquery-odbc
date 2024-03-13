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

#include "google/cloud/odbc/bq_driver/internal/odbc_sql_fns.h"
#include <cstring>

// The semantics here is coupled with SQL_FUNC_EXISTS defined in the driver
// manager. If the driver manager macro changes, this will need to change as
// well.
#define ENABLE_SQL_FUNCTION_BIT(pSupported, uwAPI) \
  (*(((UWORD*)(pSupported)) + ((uwAPI) >> 4)) |= (1 << ((uwAPI) & 0x000F)))

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;

std::map<UWORD, bool> odbc_2_fns = {{SQL_API_SQLERROR, false},
                                    {SQL_API_SQLPARAMOPTIONS, false},
                                    {SQL_API_SQLSETSCROLLOPTIONS, false},
                                    {SQL_API_SQLSETPARAM, false},
                                    {SQL_API_SQLALLOCCONNECT, false},
                                    {SQL_API_SQLALLOCENV, false},
                                    {SQL_API_SQLALLOCSTMT, false},
                                    {SQL_API_SQLFREECONNECT, false},
                                    {SQL_API_SQLFREEENV, false},
                                    {SQL_API_SQLFREESTMT, false},
                                    {SQL_API_SQLBINDPARAMETER, false},
                                    {SQL_API_SQLGETCONNECTOPTION, false},
                                    {SQL_API_SQLGETSTMTOPTION, false},
                                    {SQL_API_SQLSETCONNECTOPTION, false},
                                    {SQL_API_SQLSETSTMTOPTION, false},
                                    {SQL_API_SQLTRANSACT, false}};

std::map<UWORD, bool> odbc_3_fns = {
    {SQL_API_SQLALLOCHANDLE, true},      {SQL_API_SQLGETDESCFIELD, true},
    {SQL_API_SQLSETCONNECTATTR, true},   {SQL_API_SQLDRIVERS, true},
    {SQL_API_SQLBINDCOL, true},          {SQL_API_SQLGETDESCREC, true},
    {SQL_API_SQLCANCEL, true},           {SQL_API_SQLGETDIAGFIELD, true},
    {SQL_API_SQLCLOSECURSOR, true},      {SQL_API_SQLGETDIAGREC, true},
    {SQL_API_SQLCOLATTRIBUTE, true},     {SQL_API_SQLGETENVATTR, true},
    {SQL_API_SQLCONNECT, true},          {SQL_API_SQLGETFUNCTIONS, true},
    {SQL_API_SQLCOPYDESC, true},         {SQL_API_SQLGETINFO, true},
    {SQL_API_SQLDATASOURCES, true},      {SQL_API_SQLGETSTMTATTR, true},
    {SQL_API_SQLDESCRIBECOL, true},      {SQL_API_SQLGETTYPEINFO, true},
    {SQL_API_SQLDISCONNECT, true},       {SQL_API_SQLNUMRESULTCOLS, true},
    {SQL_API_SQLPARAMDATA, true},        {SQL_API_SQLENDTRAN, true},
    {SQL_API_SQLPREPARE, true},          {SQL_API_SQLEXECDIRECT, true},
    {SQL_API_SQLPUTDATA, true},          {SQL_API_SQLEXECUTE, true},
    {SQL_API_SQLROWCOUNT, true},         {SQL_API_SQLFETCH, true},
    {SQL_API_SQLFETCHSCROLL, true},      {SQL_API_SQLSETCURSORNAME, true},
    {SQL_API_SQLFREEHANDLE, true},       {SQL_API_SQLSETDESCFIELD, true},
    {SQL_API_SQLSETDESCREC, true},       {SQL_API_SQLGETCONNECTATTR, true},
    {SQL_API_SQLSETENVATTR, true},       {SQL_API_SQLGETCURSORNAME, true},
    {SQL_API_SQLSETSTMTATTR, true},      {SQL_API_SQLGETDATA, true},
    {SQL_API_SQLCOLUMNS, true},          {SQL_API_SQLSTATISTICS, true},
    {SQL_API_SQLSPECIALCOLUMNS, true},   {SQL_API_SQLTABLES, true},
    {SQL_API_SQLBINDPARAM, true},        {SQL_API_SQLNATIVESQL, true},
    {SQL_API_SQLBROWSECONNECT, true},    {SQL_API_SQLNUMPARAMS, true},
    {SQL_API_SQLPRIMARYKEYS, true},      {SQL_API_SQLCOLUMNPRIVILEGES, true},
    {SQL_API_SQLPROCEDURECOLUMNS, true}, {SQL_API_SQLDESCRIBEPARAM, true},
    {SQL_API_SQLPROCEDURES, true},       {SQL_API_SQLDRIVERCONNECT, true},
    {SQL_API_SQLFOREIGNKEYS, true},      {SQL_API_SQLTABLEPRIVILEGES, true},
    {SQL_API_SQLMORERESULTS, true},      {SQL_API_SQLPROCEDURES, true},
    {SQL_API_SQLSETPOS, false},          {SQL_API_SQLBULKOPERATIONS, false}};

odbc_internal::StatusRecord PopulateSupportedODBC3Functions(
    SQLUSMALLINT* supportedFunction) {
  if (!supportedFunction) {
    return StatusRecord{SQLStates::k_HY024(),
                        "Argument supportedFunction cannot be null"};
  }
  // clear memory.
  memset(supportedFunction, '\0', SQL_API_ODBC3_ALL_FUNCTIONS_SIZE);

  // Populate ODBC 2 functions first.
  for (int i = SQL_ODBC2_API_START; i <= SQL_ODBC2_API_LAST; i++) {
    bool val = IsOdbcFunctionIdSupported(static_cast<UWORD>(i));
    if (val) {
      ENABLE_SQL_FUNCTION_BIT(supportedFunction, i);
    }
  }

  // Populate ODBC 3 functions.
  for (int i = SQL_ODBC3_API_START; i <= SQL_ODBC3_API_LAST; i++) {
    bool val = IsOdbcFunctionIdSupported(static_cast<UWORD>(i));
    if (val) {
      ENABLE_SQL_FUNCTION_BIT(supportedFunction, i);
    }
  }
  return StatusRecord::Ok();
}

odbc_internal::StatusRecord PopulateSupportedODBC2Functions(
    SQLUSMALLINT* supportedFunction) {
  if (!supportedFunction) {
    return StatusRecord{SQLStates::k_HY024(),
                        "Argument supportedFunction cannot be null"};
  }
  // Populate ODBC 2 functions only.
  for (int i = SQL_ODBC2_API_START; i <= SQL_ODBC2_API_LAST; i++) {
    supportedFunction[i] = static_cast<SQLUSMALLINT>(
        IsOdbcFunctionIdSupported(static_cast<UWORD>(i)));
  }
  return StatusRecord::Ok();
}

bool IsOdbcFunctionIdSupported(UWORD fid) {
  // First check odbc 3 functions.
  auto item_found = odbc_3_fns.find(fid);
  if (item_found != odbc_3_fns.end()) {
    return item_found->second;
  }
  // Next check odbc 2 functions.
  item_found = odbc_2_fns.find(fid);
  if (item_found != odbc_2_fns.end()) {
    return item_found->second;
  }
  // function id is neither odbc 3 or odbc 2.
  return false;
}

bool IsFunctionIdOdbc3(UWORD fid) {
  return (odbc_3_fns.find(fid) != odbc_3_fns.end());
}

bool IsFunctionIdOdbc2(UWORD fid) {
  return (odbc_2_fns.find(fid) != odbc_2_fns.end());
}

}  // namespace google::cloud::odbc_bq_driver_internal
