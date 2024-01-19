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

std::map<UWORD, int> odbc_2_fns = {{SQL_API_SQLERROR, FALSE},
                                   {SQL_API_SQLPARAMOPTIONS, FALSE},
                                   {SQL_API_SQLSETSCROLLOPTIONS, FALSE},
                                   {SQL_API_SQLSETPARAM, FALSE},
                                   {SQL_API_SQLALLOCCONNECT, FALSE},
                                   {SQL_API_SQLALLOCENV, FALSE},
                                   {SQL_API_SQLALLOCSTMT, FALSE},
                                   {SQL_API_SQLFREECONNECT, FALSE},
                                   {SQL_API_SQLFREEENV, FALSE},
                                   {SQL_API_SQLFREESTMT, FALSE},
                                   {SQL_API_SQLBINDPARAMETER, FALSE},
                                   {SQL_API_SQLGETCONNECTOPTION, FALSE},
                                   {SQL_API_SQLGETSTMTOPTION, FALSE},
                                   {SQL_API_SQLSETCONNECTOPTION, FALSE},
                                   {SQL_API_SQLSETSTMTOPTION, FALSE},
                                   {SQL_API_SQLTRANSACT, FALSE}};

std::map<UWORD, int> odbc_3_fns = {
    {SQL_API_SQLALLOCHANDLE, TRUE},      {SQL_API_SQLGETDESCFIELD, TRUE},
    {SQL_API_SQLSETCONNECTATTR, TRUE},   {SQL_API_SQLDRIVERS, TRUE},
    {SQL_API_SQLBINDCOL, TRUE},          {SQL_API_SQLGETDESCREC, TRUE},
    {SQL_API_SQLCANCEL, TRUE},           {SQL_API_SQLGETDIAGFIELD, TRUE},
    {SQL_API_SQLCLOSECURSOR, TRUE},      {SQL_API_SQLGETDIAGREC, TRUE},
    {SQL_API_SQLCOLATTRIBUTE, TRUE},     {SQL_API_SQLGETENVATTR, TRUE},
    {SQL_API_SQLCONNECT, TRUE},          {SQL_API_SQLGETFUNCTIONS, TRUE},
    {SQL_API_SQLCOPYDESC, TRUE},         {SQL_API_SQLGETINFO, TRUE},
    {SQL_API_SQLDATASOURCES, TRUE},      {SQL_API_SQLGETSTMTATTR, TRUE},
    {SQL_API_SQLDESCRIBECOL, TRUE},      {SQL_API_SQLGETTYPEINFO, TRUE},
    {SQL_API_SQLDISCONNECT, TRUE},       {SQL_API_SQLNUMRESULTCOLS, TRUE},
    {SQL_API_SQLPARAMDATA, TRUE},        {SQL_API_SQLENDTRAN, TRUE},
    {SQL_API_SQLPREPARE, TRUE},          {SQL_API_SQLEXECDIRECT, TRUE},
    {SQL_API_SQLPUTDATA, TRUE},          {SQL_API_SQLEXECUTE, TRUE},
    {SQL_API_SQLROWCOUNT, TRUE},         {SQL_API_SQLFETCH, TRUE},
    {SQL_API_SQLFETCHSCROLL, TRUE},      {SQL_API_SQLSETCURSORNAME, TRUE},
    {SQL_API_SQLFREEHANDLE, TRUE},       {SQL_API_SQLSETDESCFIELD, TRUE},
    {SQL_API_SQLSETDESCREC, TRUE},       {SQL_API_SQLGETCONNECTATTR, TRUE},
    {SQL_API_SQLSETENVATTR, TRUE},       {SQL_API_SQLGETCURSORNAME, TRUE},
    {SQL_API_SQLSETSTMTATTR, TRUE},      {SQL_API_SQLGETDATA, TRUE},
    {SQL_API_SQLCOLUMNS, TRUE},          {SQL_API_SQLSTATISTICS, TRUE},
    {SQL_API_SQLSPECIALCOLUMNS, TRUE},   {SQL_API_SQLTABLES, TRUE},
    {SQL_API_SQLBINDPARAM, TRUE},        {SQL_API_SQLNATIVESQL, TRUE},
    {SQL_API_SQLBROWSECONNECT, TRUE},    {SQL_API_SQLNUMPARAMS, TRUE},
    {SQL_API_SQLPRIMARYKEYS, TRUE},      {SQL_API_SQLCOLUMNPRIVILEGES, TRUE},
    {SQL_API_SQLPROCEDURECOLUMNS, TRUE}, {SQL_API_SQLDESCRIBEPARAM, TRUE},
    {SQL_API_SQLPROCEDURES, TRUE},       {SQL_API_SQLDRIVERCONNECT, TRUE},
    {SQL_API_SQLFOREIGNKEYS, TRUE},      {SQL_API_SQLTABLEPRIVILEGES, TRUE},
    {SQL_API_SQLMORERESULTS, TRUE},      {SQL_API_SQLPROCEDURES, TRUE},
    {SQL_API_SQLSETPOS, FALSE},          {SQL_API_SQLBULKOPERATIONS, FALSE}};

Status PopulateSupportedODBC3Functions(TraceOptions& opts,
                                       SQLUSMALLINT* supportedFunction) {
  if (!supportedFunction) {
    TracePrintInternal(opts,
                       "Internal error: supportedFunction pointer is null!");
    // TODO(b/308656768,b/308656826): Record error or diagnostic info for
    // SQLDiagRec and/or SQLDiagField.
    return Status(StatusCode::kInvalidArgument,
                  "Argument supportedFunction cannot be null");
  }
  // clear memory.
  memset(supportedFunction, '\0', SQL_API_ODBC3_ALL_FUNCTIONS_SIZE);

  // Populate ODBC 2 functions first.
  for (int i = SQL_ODBC2_API_START; i <= SQL_ODBC2_API_LAST; i++) {
    int val = IsOdbcFunctionIdSupported((UWORD)i);
    if (val == TRUE) {
      ENABLE_SQL_FUNCTION_BIT(supportedFunction, i);
    }
  }

  // Populate ODBC 3 functions.
  for (int i = SQL_ODBC3_API_START; i <= SQL_ODBC3_API_LAST; i++) {
    int val = IsOdbcFunctionIdSupported((UWORD)i);
    if (val == TRUE) {
      ENABLE_SQL_FUNCTION_BIT(supportedFunction, i);
    }
  }
  return Status(StatusCode::kOk, "");
}

Status PopulateSupportedODBC2Functions(TraceOptions& opts,
                                       SQLUSMALLINT* supportedFunction) {
  if (!supportedFunction) {
    TracePrintInternal(opts,
                       "Internal error: supportedFunction pointer is null!");
    // TODO(b/308656768,b/308656826): Record error or diagnostic info for
    // SQLDiagRec and/or SQLDiagField.
    return Status(StatusCode::kInvalidArgument,
                  "Argument supportedFunction cannot be null");
  }
  // Populate ODBC 2 functions only.
  for (int i = SQL_ODBC2_API_START; i <= SQL_ODBC2_API_LAST; i++) {
    supportedFunction[i] = IsOdbcFunctionIdSupported((UWORD)i);
    ;
  }
  return Status(StatusCode::kOk, "");
}

int IsOdbcFunctionIdSupported(UWORD fid) {
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
  return FALSE;
}

bool IsFunctionIdOdbc3(UWORD fid) {
  if (odbc_3_fns.find(fid) != odbc_3_fns.end()) {
    return true;
  }
  return false;
}

bool IsFunctionIdOdbc2(UWORD fid) {
  if (odbc_2_fns.find(fid) != odbc_2_fns.end()) {
    return true;
  }
  return false;
}

}  // namespace google::cloud::odbc_bq_driver_internal
