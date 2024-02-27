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

#include "google/cloud/odbc/bq_driver/odbc_driver_metadata.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_fns.h"
#include "google/cloud/odbc/bq_driver/odbc_commons.h"
#include "google/cloud/odbc/bq_driver/odbc_environment.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver {

using ::google::cloud::odbc_bq_driver::SQLAllocConnHandle;
using ::google::cloud::odbc_bq_driver::SQLAllocEnvHandle;
using ::google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using ::google::cloud::odbc_bq_driver_internal::kSqlApiAllFuncsSize;
using ::google::cloud::odbc_bq_driver_internal::Section;
using ::google::cloud::odbc_bq_driver_internal::TraceOptions;

using google::cloud::odbc_testing_utils::StatusIs;
using ::testing::HasSubstr;

std::string const kDsnDescription = "test-dsn";
std::string const kDsnCatalog = "bigquery-test";
std::string const kDsnDriver = "test-driver";
std::string const kDsnName = "SampleDSN";

// Helper class and functions specific to odbc metadata unit tests.
namespace {
class OdbcMetadataConnectionHandleTest : public ConnectionHandle {
 public:
  explicit OdbcMetadataConnectionHandleTest() = default;
  void SetConnected() { is_connected_ = true; }
};

OdbcMetadataConnectionHandleTest* connection_handle = nullptr;
HandleWrapped* handle_wrapped = nullptr;

void CreateConnHandle(HandleType const& type, bool connected, bool setup_dsn) {
  connection_handle = new OdbcMetadataConnectionHandleTest();
  if (connected) {
    connection_handle->SetConnected();
  }
  if (setup_dsn) {
    Section dsn_section;
    dsn_section["Description"] = kDsnDescription;
    dsn_section["Driver"] = kDsnDriver;
    dsn_section["Catalog"] = kDsnCatalog;
    connection_handle->SetUp(dsn_section, kDsnName);
  }
  handle_wrapped = new HandleWrapped(type, connection_handle);
}

void CreateConnectedHandle(HandleType const& type) {
  return CreateConnHandle(type, /* connected= */ true,
                          /*setup_dsn=*/false);
}

void CreateDisconnectedHandle(HandleType const& type) {
  return CreateConnHandle(type, /* connected= */ false,
                          /*setup_dsn=*/false);
}

void CreateConnectedHandleWithDsn(HandleType const& type) {
  return CreateConnHandle(type, /* connected= */ true,
                          /*setup_dsn=*/true);
}

void FreeHandles() {
  if (connection_handle) {
    delete connection_handle;
  }
  if (handle_wrapped) {
    delete handle_wrapped;
  }
}

}  // namespace

TEST(SQLGetFunctionsInternal, AllSupportedOdbc3Functions) {
  SQLUSMALLINT odbc3_fns[SQL_API_ODBC3_ALL_FUNCTIONS_SIZE];
  CreateConnectedHandle(HandleType::kConnHandle);
  ASSERT_TRUE(connection_handle != nullptr);
  ASSERT_TRUE(handle_wrapped != nullptr);

  SQLRETURN rc = SQLGetFunctionsInternal(
      handle_wrapped, SQL_API_ODBC3_ALL_FUNCTIONS, odbc3_fns);
  EXPECT_EQ(SQL_SUCCESS, rc);

  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLALLOCHANDLE));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLGETDESCFIELD));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLSETCONNECTATTR));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLDRIVERS));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLBINDCOL));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLGETDESCREC));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLCANCEL));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLGETDIAGFIELD));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLCLOSECURSOR));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLGETDIAGREC));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLCOLATTRIBUTE));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLGETENVATTR));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLCONNECT));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLGETFUNCTIONS));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLCOPYDESC));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLGETINFO));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLDATASOURCES));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLGETSTMTATTR));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLDESCRIBECOL));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLGETTYPEINFO));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLDISCONNECT));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLNUMRESULTCOLS));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLPARAMDATA));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLENDTRAN));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLPREPARE));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLEXECDIRECT));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLPUTDATA));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLEXECUTE));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLROWCOUNT));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLFETCH));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLFETCHSCROLL));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLSETCURSORNAME));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLFREEHANDLE));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLSETDESCFIELD));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLSETDESCREC));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLGETCONNECTATTR));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLSETENVATTR));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLGETCURSORNAME));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLSETSTMTATTR));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLGETDATA));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLCOLUMNS));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLSTATISTICS));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLSPECIALCOLUMNS));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLTABLES));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLBINDPARAM));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLNATIVESQL));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLBROWSECONNECT));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLNUMPARAMS));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLPRIMARYKEYS));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLCOLUMNPRIVILEGES));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLPROCEDURECOLUMNS));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLDESCRIBEPARAM));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLPROCEDURES));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLDRIVERCONNECT));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLFOREIGNKEYS));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLTABLEPRIVILEGES));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLMORERESULTS));
  FreeHandles();
}

TEST(SQLGetFunctionsInternal, AllUnSupportedOdbc3Functions) {
  SQLUSMALLINT odbc3_fns[SQL_API_ODBC3_ALL_FUNCTIONS_SIZE];

  CreateConnectedHandle(HandleType::kConnHandle);
  ASSERT_TRUE(connection_handle != nullptr);
  ASSERT_TRUE(handle_wrapped != nullptr);

  SQLRETURN rc = SQLGetFunctionsInternal(
      handle_wrapped, SQL_API_ODBC3_ALL_FUNCTIONS, odbc3_fns);
  EXPECT_EQ(SQL_SUCCESS, rc);

  EXPECT_EQ(SQL_FALSE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLBULKOPERATIONS));
  EXPECT_EQ(SQL_FALSE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLSETPOS));
  FreeHandles();
}

TEST(SQLGetFunctionsInternal, ODBC3FunctionIdSupported) {
  SQLUSMALLINT supported;

  CreateConnectedHandle(HandleType::kConnHandle);
  ASSERT_TRUE(connection_handle != nullptr);
  ASSERT_TRUE(handle_wrapped != nullptr);

  SQLRETURN rc = SQLGetFunctionsInternal(handle_wrapped, SQL_API_SQLMORERESULTS,
                                         &supported);
  EXPECT_EQ(SQL_SUCCESS, rc);
  EXPECT_EQ(SQL_TRUE, supported);
  FreeHandles();
}

TEST(SQLGetFunctionsInternal, ODBC3FunctionIdNotSupported) {
  SQLUSMALLINT supported;

  CreateConnectedHandle(HandleType::kConnHandle);
  ASSERT_TRUE(connection_handle != nullptr);
  ASSERT_TRUE(handle_wrapped != nullptr);

  SQLRETURN rc =
      SQLGetFunctionsInternal(handle_wrapped, SQL_API_SQLSETPOS, &supported);
  EXPECT_EQ(SQL_SUCCESS, rc);
  EXPECT_EQ(SQL_FALSE, supported);
  FreeHandles();
}

TEST(SQLGetFunctionsInternal, ODBC2FunctionIdNotSupported) {
  SQLUSMALLINT supported;

  CreateConnectedHandle(HandleType::kConnHandle);
  ASSERT_TRUE(connection_handle != nullptr);
  ASSERT_TRUE(handle_wrapped != nullptr);
  SQLRETURN rc =
      SQLGetFunctionsInternal(handle_wrapped, SQL_API_SQLERROR, &supported);
  EXPECT_EQ(SQL_SUCCESS, rc);
  EXPECT_EQ(SQL_FALSE, supported);
  FreeHandles();
}

TEST(SQLGetFunctionsInternal, AllUnSupportedOdbc2Functions) {
  SQLUSMALLINT odbc2_fns[kSqlApiAllFuncsSize];

  CreateConnectedHandle(HandleType::kConnHandle);
  ASSERT_TRUE(connection_handle != nullptr);
  ASSERT_TRUE(handle_wrapped != nullptr);

  SQLRETURN rc =
      SQLGetFunctionsInternal(handle_wrapped, SQL_API_ALL_FUNCTIONS, odbc2_fns);
  EXPECT_EQ(SQL_SUCCESS, rc);

  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLERROR]);
  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLPARAMOPTIONS]);
  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLSETSCROLLOPTIONS]);
  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLSETPARAM]);
  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLALLOCCONNECT]);
  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLALLOCENV]);
  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLALLOCSTMT]);
  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLFREECONNECT]);
  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLFREEENV]);
  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLFREESTMT]);
  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLBINDPARAMETER]);
  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLGETCONNECTOPTION]);
  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLGETSTMTOPTION]);
  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLSETCONNECTOPTION]);
  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLSETSTMTOPTION]);
  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLTRANSACT]);

  FreeHandles();
}

TEST(SQLGetFunctionsInternal, Odbc2NullConnectionHandle) {
  SQLUSMALLINT odbc2_fns[kSqlApiAllFuncsSize];
  SQLRETURN rc =
      SQLGetFunctionsInternal(nullptr, SQL_API_ALL_FUNCTIONS, odbc2_fns);
  EXPECT_EQ(SQL_INVALID_HANDLE, rc);
}

TEST(SQLGetFunctionsInternal, Odbc3NullConnectionHandle) {
  SQLUSMALLINT odbc3_fns[SQL_API_ODBC3_ALL_FUNCTIONS_SIZE];
  SQLRETURN rc =
      SQLGetFunctionsInternal(nullptr, SQL_API_ODBC3_ALL_FUNCTIONS, odbc3_fns);
  EXPECT_EQ(SQL_INVALID_HANDLE, rc);
}

TEST(SQLGetFunctionsInternal, Odbc2InvalidConnectionHandleType) {
  CreateConnectedHandle(HandleType::kEnvHandle);
  ASSERT_TRUE(connection_handle != nullptr);
  ASSERT_TRUE(handle_wrapped != nullptr);
  SQLRETURN rc =
      SQLGetFunctionsInternal(handle_wrapped, SQL_API_ALL_FUNCTIONS, nullptr);
  EXPECT_EQ(SQL_INVALID_HANDLE, rc);
  FreeHandles();
}

TEST(SQLGetFunctionsInternal, Odbc3InvalidConnectionHandleType) {
  CreateConnectedHandle(HandleType::kEnvHandle);
  ASSERT_TRUE(connection_handle != nullptr);
  ASSERT_TRUE(handle_wrapped != nullptr);
  SQLRETURN rc = SQLGetFunctionsInternal(handle_wrapped,
                                         SQL_API_ODBC3_ALL_FUNCTIONS, nullptr);
  EXPECT_EQ(SQL_INVALID_HANDLE, rc);
  FreeHandles();
}

TEST(SQLGetFunctionsInternal, ConnectionHandleNotConnectedFailure) {
  CreateDisconnectedHandle(HandleType::kConnHandle);
  ASSERT_TRUE(connection_handle != nullptr);
  ASSERT_TRUE(handle_wrapped != nullptr);

  SQLRETURN rc = SQLGetFunctionsInternal(handle_wrapped,
                                         SQL_API_ODBC3_ALL_FUNCTIONS, nullptr);
  EXPECT_EQ(SQL_INVALID_HANDLE, rc);
  FreeHandles();
}

TEST(SQLGetInfoInternal, HandleConnectionInfoTypes_DSN_Name) {
  SQLCHAR* dest = new SQLCHAR[256];
  SQLSMALLINT in_buffer_len = 256;
  SQLSMALLINT str_len_ptr;
  CreateConnectedHandleWithDsn(HandleType::kConnHandle);
  ASSERT_TRUE(connection_handle != nullptr);
  ASSERT_TRUE(handle_wrapped != nullptr);
  ASSERT_EQ(SQL_SUCCESS,
            SQLGetInfoInternal(handle_wrapped, SQL_DATA_SOURCE_NAME,
                               reinterpret_cast<SQLPOINTER>(dest),
                               in_buffer_len, &str_len_ptr));

  std::string actual = reinterpret_cast<char*>(dest);
  EXPECT_EQ(kDsnName, actual);
  EXPECT_EQ(str_len_ptr, 9);
  delete[] dest;
  FreeHandles();
}

TEST(SQLGetInfoInternal, HandleConnectionInfoTypes_Database_Name) {
  SQLCHAR* dest = new SQLCHAR[256];
  SQLSMALLINT in_buffer_len = 256;
  SQLSMALLINT str_len_ptr;
  CreateConnectedHandleWithDsn(HandleType::kConnHandle);
  ASSERT_TRUE(connection_handle != nullptr);
  ASSERT_TRUE(handle_wrapped != nullptr);
  ASSERT_EQ(SQL_SUCCESS, SQLGetInfoInternal(handle_wrapped, SQL_DATABASE_NAME,
                                            reinterpret_cast<SQLPOINTER>(dest),
                                            in_buffer_len, &str_len_ptr));

  std::string actual = reinterpret_cast<char*>(dest);
  EXPECT_EQ(kDsnCatalog, actual);
  EXPECT_EQ(str_len_ptr, 13);
  delete[] dest;
  FreeHandles();
}

TEST(SQLGetInfoInternal, SQLGetInfoCharSupported) {
  SQLCHAR* dest = new SQLCHAR[10];
  SQLSMALLINT in_buffer_len = 10;
  SQLSMALLINT str_len_ptr;
  CreateConnectedHandle(HandleType::kConnHandle);
  ASSERT_TRUE(connection_handle != nullptr);
  ASSERT_TRUE(handle_wrapped != nullptr);
  ASSERT_EQ(SQL_SUCCESS, SQLGetInfoInternal(handle_wrapped, SQL_CATALOG_NAME,
                                            reinterpret_cast<SQLPOINTER>(dest),
                                            in_buffer_len, &str_len_ptr));

  std::string actual = reinterpret_cast<char*>(dest);
  EXPECT_EQ("Y", actual);
  EXPECT_EQ(str_len_ptr, 1);
  delete[] dest;
  FreeHandles();
}

TEST(SQLGetInfoInternal, NotConnectedFailure) {
  SQLCHAR* dest = new SQLCHAR[10];
  SQLSMALLINT in_buffer_len = 10;
  SQLSMALLINT str_len_ptr;
  CreateDisconnectedHandle(HandleType::kConnHandle);
  ASSERT_TRUE(connection_handle != nullptr);
  ASSERT_TRUE(handle_wrapped != nullptr);
  SQLRETURN rc = SQLGetInfoInternal(handle_wrapped, SQL_DATABASE_NAME,
                                    reinterpret_cast<SQLPOINTER>(dest),
                                    in_buffer_len, &str_len_ptr);

  EXPECT_EQ(SQL_INVALID_HANDLE, rc);
  delete[] dest;
  FreeHandles();
}

TEST(SQLGetInfoInternal, SQLGetInfoCharUnSupported) {
  SQLCHAR* dest = new SQLCHAR[10];
  SQLSMALLINT in_buffer_len = 10;
  SQLSMALLINT str_len_ptr;
  CreateConnectedHandle(HandleType::kConnHandle);
  ASSERT_TRUE(connection_handle != nullptr);
  ASSERT_TRUE(handle_wrapped != nullptr);
  ASSERT_EQ(SQL_SUCCESS,
            SQLGetInfoInternal(handle_wrapped, SQL_ACCESSIBLE_PROCEDURES,
                               reinterpret_cast<SQLPOINTER>(dest),
                               in_buffer_len, &str_len_ptr));

  std::string actual = reinterpret_cast<char*>(dest);
  EXPECT_EQ("N", actual);
  EXPECT_EQ(str_len_ptr, 1);
  delete[] dest;
  FreeHandles();
}

TEST(SQLGetInfoInternal, SQLGetInfoUSmallIntSupported) {
  SQLUSMALLINT dest;
  SQLSMALLINT in_buffer_len = 0;
  SQLSMALLINT str_len_ptr;
  CreateConnectedHandle(HandleType::kConnHandle);
  ASSERT_TRUE(connection_handle != nullptr);
  ASSERT_TRUE(handle_wrapped != nullptr);
  ASSERT_EQ(SQL_SUCCESS,
            SQLGetInfoInternal(handle_wrapped, SQL_CATALOG_LOCATION,
                               reinterpret_cast<SQLPOINTER>(&dest),
                               in_buffer_len, &str_len_ptr));

  EXPECT_EQ(SQL_CL_START, dest);
  EXPECT_EQ(str_len_ptr, 2);
  FreeHandles();
}

TEST(SQLGetInfoInternal, SQLGetInfoUSmallIntUnSupported) {
  SQLUSMALLINT dest;
  SQLSMALLINT in_buffer_len = 0;
  SQLSMALLINT str_len_ptr;
  CreateConnectedHandle(HandleType::kConnHandle);
  ASSERT_TRUE(connection_handle != nullptr);
  ASSERT_TRUE(handle_wrapped != nullptr);
  ASSERT_EQ(SQL_SUCCESS,
            SQLGetInfoInternal(handle_wrapped, SQL_ACTIVE_ENVIRONMENTS,
                               reinterpret_cast<SQLPOINTER>(&dest),
                               in_buffer_len, &str_len_ptr));

  EXPECT_EQ(0, dest);
  EXPECT_EQ(str_len_ptr, 2);
  FreeHandles();
}

TEST(SQLGetInfoInternal, SQLGetInfoUIntSupported) {
  SQLUINTEGER dest;
  SQLSMALLINT in_buffer_len = 0;
  SQLSMALLINT str_len_ptr;
  CreateConnectedHandle(HandleType::kConnHandle);
  ASSERT_TRUE(connection_handle != nullptr);
  ASSERT_TRUE(handle_wrapped != nullptr);
  ASSERT_EQ(SQL_SUCCESS,
            SQLGetInfoInternal(handle_wrapped, SQL_DEFAULT_TXN_ISOLATION,
                               reinterpret_cast<SQLPOINTER>(&dest),
                               in_buffer_len, &str_len_ptr));

  EXPECT_EQ(SQL_TXN_SERIALIZABLE, dest);
  EXPECT_EQ(str_len_ptr, 4);
  FreeHandles();
}

TEST(SQLGetInfoInternal, SQLGetInfoUIntUnSupported) {
  SQLUINTEGER dest;
  SQLSMALLINT in_buffer_len = 0;
  SQLSMALLINT str_len_ptr;
  CreateConnectedHandle(HandleType::kConnHandle);
  ASSERT_TRUE(connection_handle != nullptr);
  ASSERT_TRUE(handle_wrapped != nullptr);
  ASSERT_EQ(SQL_SUCCESS, SQLGetInfoInternal(handle_wrapped, SQL_BATCH_ROW_COUNT,
                                            reinterpret_cast<SQLPOINTER>(&dest),
                                            in_buffer_len, &str_len_ptr));

  EXPECT_EQ(0, dest);
  EXPECT_EQ(str_len_ptr, 4);
  FreeHandles();
}

TEST(SQLGetInfoInternal, SQLGetInfoBitmaskSupported) {
  SQLUINTEGER dest;
  SQLSMALLINT in_buffer_len = 0;
  SQLSMALLINT str_len_ptr;
  CreateConnectedHandle(HandleType::kConnHandle);
  ASSERT_TRUE(connection_handle != nullptr);
  ASSERT_TRUE(handle_wrapped != nullptr);
  ASSERT_EQ(SQL_SUCCESS, SQLGetInfoInternal(handle_wrapped, SQL_CATALOG_USAGE,
                                            reinterpret_cast<SQLPOINTER>(&dest),
                                            in_buffer_len, &str_len_ptr));

  EXPECT_EQ(SQL_CU_DML_STATEMENTS, dest);
  EXPECT_EQ(str_len_ptr, 4);
  FreeHandles();
}

TEST(SQLGetInfoInternal, SQLGetInfoBitmaskIntUnSupported) {
  SQLUINTEGER dest;
  SQLSMALLINT in_buffer_len = 0;
  SQLSMALLINT str_len_ptr;
  CreateConnectedHandle(HandleType::kConnHandle);
  ASSERT_TRUE(connection_handle != nullptr);
  ASSERT_TRUE(handle_wrapped != nullptr);
  ASSERT_EQ(SQL_SUCCESS, SQLGetInfoInternal(handle_wrapped, SQL_ALTER_DOMAIN,
                                            reinterpret_cast<SQLPOINTER>(&dest),
                                            in_buffer_len, &str_len_ptr));

  EXPECT_EQ(0L, dest);
  EXPECT_EQ(str_len_ptr, 4);
  FreeHandles();
}

TEST(SQLGetInfoInternal, InfoValueNullPtr) {
  SQLSMALLINT in_buffer_len = 0;
  SQLSMALLINT str_len_ptr;
  CreateConnectedHandle(HandleType::kConnHandle);
  ASSERT_TRUE(connection_handle != nullptr);
  ASSERT_TRUE(handle_wrapped != nullptr);
  SQLRETURN rc = SQLGetInfoInternal(handle_wrapped, SQL_ALTER_DOMAIN, nullptr,
                                    in_buffer_len, &str_len_ptr);
  EXPECT_EQ(SQL_ERROR, rc);
  FreeHandles();
}

}  // namespace google::cloud::odbc_bq_driver
