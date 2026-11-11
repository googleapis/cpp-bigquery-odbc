// Copyright 2026 Google LLC
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

#include "google/cloud/odbc/testing/odbc_utils/connection.h"
#include "google/cloud/odbc/testing/odbc_utils/statement.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

namespace google::cloud::odbc_tests {

#ifdef BQ_DRIVER_INTEGRATION_TESTS

struct IdleProxyConfig {
  std::string proxy_host;
  std::string proxy_port;
  std::chrono::seconds pause{45};
  std::int64_t rows = 60000;
  std::string expect = "success";
  std::string kills_file;
  std::string stats_file;
};

struct IdleProxyStats {
  int kills = 0;
  int server_drops = 0;
  int ping_floods = 0;
  int reconnects = 0;
  int storage_tunnels = 0;
};

inline std::optional<std::string> GetEnvVar(char const* name) {
  char const* val = std::getenv(name);
  if (!val || std::string_view(val).empty()) {
    return std::nullopt;
  }
  return std::string(val);
}

inline std::optional<IdleProxyConfig> GetIdleProxyConfig() {
  auto proxy = GetEnvVar("BQ_ODBC_IDLE_PROXY");
  if (!proxy) {
    return std::nullopt;
  }
  IdleProxyConfig config;
  auto colon = proxy->find(':');
  if (colon != std::string::npos) {
    config.proxy_host = proxy->substr(0, colon);
    config.proxy_port = proxy->substr(colon + 1);
  } else {
    config.proxy_host = *proxy;
    config.proxy_port = "8888";
  }
  if (auto pause_str = GetEnvVar("BQ_ODBC_IDLE_PROXY_PAUSE_SECONDS")) {
    config.pause = std::chrono::seconds(std::stoi(*pause_str));
  }
  if (auto rows_str = GetEnvVar("BQ_ODBC_IDLE_PROXY_ROWS")) {
    config.rows = std::stoll(*rows_str);
  }
  if (auto expect_str = GetEnvVar("BQ_ODBC_IDLE_PROXY_EXPECT")) {
    config.expect = *expect_str;
  }
  if (auto kills_str = GetEnvVar("BQ_ODBC_IDLE_PROXY_KILLS_FILE")) {
    config.kills_file = *kills_str;
  } else {
#ifdef _WIN32
    char const* temp = std::getenv("TEMP");
    config.kills_file =
        (temp ? std::string(temp) : "C:\\Temp") + "\\idle_connect_proxy.kills";
#else
    config.kills_file = "/tmp/idle_connect_proxy.kills";
#endif
  }
  if (auto stats_str = GetEnvVar("BQ_ODBC_IDLE_PROXY_STATS_FILE")) {
    config.stats_file = *stats_str;
  } else {
#ifdef _WIN32
    char const* temp = std::getenv("TEMP");
    config.stats_file =
        (temp ? std::string(temp) : "C:\\Temp") + "\\idle_connect_proxy.stats";
#else
    config.stats_file = "/tmp/idle_connect_proxy.stats";
#endif
  }
  return config;
}

inline IdleProxyStats GetIdleProxyStats(std::string const& stats_file,
                                        std::string const& kills_file) {
  IdleProxyStats stats;
  std::ifstream sf(stats_file);
  if (sf.is_open()) {
    std::string line;
    while (std::getline(sf, line)) {
      std::istringstream iss(line);
      std::string key;
      int val = 0;
      if (iss >> key >> val) {
        if (key == "kills:")
          stats.kills = val;
        else if (key == "server_drops:")
          stats.server_drops = val;
        else if (key == "ping_floods:")
          stats.ping_floods = val;
        else if (key == "reconnects:")
          stats.reconnects = val;
        else if (key == "storage_tunnels:")
          stats.storage_tunnels = val;
      }
    }
  } else {
    std::ifstream kf(kills_file);
    if (kf.is_open()) {
      kf >> stats.kills;
    }
  }
  return stats;
}

inline std::string IdleProxyConnString(IdleProxyConfig const& cfg) {
  return kDefaultConnectionString +
         ";AllowHtapiForLargeResults=1;HTAPI_ActivationThreshold=0;ProxyHost=" +
         cfg.proxy_host + ";ProxyPort=" + cfg.proxy_port;
}

inline std::string IdleProxyQuery(std::int64_t rows) {
  return "SELECT id, REPEAT('x', 1000) AS payload FROM "
         "UNNEST(GENERATE_ARRAY(1, " +
         std::to_string(rows) + ")) AS id ORDER BY id";
}

TEST(ResilienceTest, ReadAPI_IdleProxy_StreamDroppedMidFetch) {
  auto const config = GetIdleProxyConfig();
  if (!config) {
    GTEST_SKIP() << "Set BQ_ODBC_IDLE_PROXY=host:port to run idle proxy tests";
  }

  IdleProxyStats const before =
      GetIdleProxyStats(config->stats_file, config->kills_file);

  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(IdleProxyConnString(*config), conn), SQL_SUCCESS);

  std::string const query = IdleProxyQuery(config->rows);
  ASSERT_EQ(SQLExecDirect(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS),
            SQL_SUCCESS);

  SQLBIGINT id = 0;
  SQLLEN id_ind = 0;
  ASSERT_EQ(SQLBindCol(conn->hstmt, 1, SQL_C_SBIGINT, &id, sizeof(id), &id_ind),
            SQL_SUCCESS);

  std::int64_t fetched = 0;
  std::int64_t const rows_before_pause =
      std::min<std::int64_t>(100, config->rows / 2);
  while (fetched < rows_before_pause) {
    SQLRETURN rc = SQLFetch(conn->hstmt);
    if (!SQL_SUCCEEDED(rc)) break;
    fetched++;
  }
  ASSERT_EQ(fetched, rows_before_pause);

  std::this_thread::sleep_for(config->pause);

  while (true) {
    SQLRETURN rc = SQLFetch(conn->hstmt);
    if (rc == SQL_NO_DATA) break;
    if (!SQL_SUCCEEDED(rc)) break;
    fetched++;
  }

  IdleProxyStats const after =
      GetIdleProxyStats(config->stats_file, config->kills_file);

  int const kills = after.kills - before.kills;
  int const server_drops = after.server_drops - before.server_drops;
  int const ping_floods = after.ping_floods - before.ping_floods;
  int const reconnects = after.reconnects - before.reconnects;
  int const storage_tunnels = after.storage_tunnels - before.storage_tunnels;

  EXPECT_EQ(fetched, config->rows);
  EXPECT_GE(storage_tunnels, 1)
      << "Expected at least one BigQuery Storage tunnel through the proxy";
  if (config->expect == "error") {
    EXPECT_TRUE(kills > 0 || server_drops > 0)
        << "Expected proxy or server connection drop in error mode";
  } else {
    EXPECT_EQ(kills, 0)
        << "Idle connect proxy killed " << kills
        << " connection(s) during idle pause! (State 1: Keepalive disabled)";
    EXPECT_EQ(server_drops, 0)
        << "Server dropped " << server_drops
        << " connection(s) during idle pause! (State 2: Ping flood rejection)";
    EXPECT_EQ(ping_floods, 0)
        << "Ping flood detected " << ping_floods
        << " times during pause! (State 2: 10ms interval)";
    EXPECT_EQ(reconnects, 0) << "Driver was forced to perform " << reconnects
                             << " emergency reconnect(s)!";
  }

  EXPECT_EQ(SQLFreeStmt(conn->hstmt, SQL_CLOSE), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(ResilienceTest, ReadAPI_IdleProxy_ConnectionDroppedBetweenQueries) {
  auto const config = GetIdleProxyConfig();
  if (!config) {
    GTEST_SKIP() << "Set BQ_ODBC_IDLE_PROXY=host:port to run idle proxy tests";
  }

  IdleProxyStats const before =
      GetIdleProxyStats(config->stats_file, config->kills_file);

  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(IdleProxyConnString(*config), conn), SQL_SUCCESS);

  std::string const query = IdleProxyQuery(config->rows);

  ASSERT_EQ(SQLExecDirect(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS),
            SQL_SUCCESS);
  std::int64_t rows1 = 0;
  while (true) {
    SQLRETURN rc = SQLFetch(conn->hstmt);
    if (rc == SQL_NO_DATA) break;
    if (!SQL_SUCCEEDED(rc)) break;
    rows1++;
  }
  EXPECT_EQ(rows1, config->rows);
  EXPECT_EQ(SQLFreeStmt(conn->hstmt, SQL_CLOSE), SQL_SUCCESS);

  std::this_thread::sleep_for(config->pause);

  ASSERT_EQ(SQLExecDirect(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS),
            SQL_SUCCESS);
  std::int64_t rows2 = 0;
  while (true) {
    SQLRETURN rc = SQLFetch(conn->hstmt);
    if (rc == SQL_NO_DATA) break;
    if (!SQL_SUCCEEDED(rc)) break;
    rows2++;
  }
  EXPECT_EQ(rows2, config->rows);

  IdleProxyStats const after =
      GetIdleProxyStats(config->stats_file, config->kills_file);

  int const kills = after.kills - before.kills;
  int const server_drops = after.server_drops - before.server_drops;
  int const ping_floods = after.ping_floods - before.ping_floods;
  int const reconnects = after.reconnects - before.reconnects;
  int const storage_tunnels = after.storage_tunnels - before.storage_tunnels;

  EXPECT_GE(storage_tunnels, 1)
      << "Expected at least one BigQuery Storage tunnel through the proxy";
  if (config->expect == "error") {
    EXPECT_TRUE(kills > 0 || server_drops > 0)
        << "Expected proxy or server connection drop in error mode";
  } else {
    EXPECT_EQ(kills, 0)
        << "Idle connect proxy killed " << kills
        << " connection(s) between queries! (State 1: Keepalive disabled)";
    EXPECT_EQ(server_drops, 0)
        << "Server dropped " << server_drops
        << " connection(s) between queries! (State 2: Ping flood rejection)";
    EXPECT_EQ(ping_floods, 0)
        << "Ping flood detected " << ping_floods
        << " times between queries! (State 2: 10ms interval)";
    EXPECT_EQ(reconnects, 0) << "Driver was forced to perform " << reconnects
                             << " emergency reconnect(s)!";
  }

  EXPECT_EQ(SQLFreeStmt(conn->hstmt, SQL_CLOSE), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SQLExecDirect_HTAPI_StreamRetryAndResume) {
  auto const config = GetIdleProxyConfig();
  if (!config) {
    GTEST_SKIP() << "Set BQ_ODBC_IDLE_PROXY=host:port to run idle proxy tests";
  }

  IdleProxyStats const before =
      GetIdleProxyStats(config->stats_file, config->kills_file);

  auto conn = std::make_shared<ODBCHandles>();
  std::string connection_string =
      IdleProxyConnString(*config) +
      ";UseDefaultLargeResultsDataset=0;"
      "LargeResultsDataSetId=_bqodbc_temp_tables_euwest1";
  ASSERT_EQ(Connect(connection_string, conn), SQL_SUCCESS);

  int const limit = 3000;
  std::string query =
      "SELECT * EXCEPT (index) FROM "
      "ODBC_HTAPI_TESTING_EUROPE_WEST1.300_columns_string "
      "ORDER BY index LIMIT " +
      std::to_string(limit) + ";";

  Table table("Random_table_name");
  RowWiseResults const& results = table.Fetch(conn, query);

  IdleProxyStats const after =
      GetIdleProxyStats(config->stats_file, config->kills_file);

  int const kills = after.kills - before.kills;
  int const reconnects = after.reconnects - before.reconnects;
  int const storage_tunnels = after.storage_tunnels - before.storage_tunnels;

  EXPECT_GE(storage_tunnels, 3)
      << "Expected initial and reconnected BigQuery Storage tunnels through "
         "the proxy";
  EXPECT_GE(kills, 2)
      << "Expected proxy to kill the active HTAPI stream mid-query";
  EXPECT_GE(reconnects, 2)
      << "Expected driver to reconnect and resume HTAPI stream after drops";

  int const expected_num_cols = 300;
  ASSERT_EQ(results.size(), limit) << "Row count mismatch.";
  for (int i = 0; i < limit; ++i) {
    Row const& row = results[i];
    ASSERT_EQ(row.size(), expected_num_cols)
        << "Row " << i << ": Column count mismatch.";
    for (int j = 0; j < expected_num_cols; ++j) {
      std::string expected_value =
          "col" + std::to_string(j) + "_row" + std::to_string(i);
      ASSERT_TRUE(row.count(j))
          << "Row " << i << ": Missing expected column with index " << j;
      ASSERT_EQ(row.at(j), expected_value)
          << "Row " << i << ", Col " << j << ": Value mismatch.";
    }
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#endif  // BQ_DRIVER_INTEGRATION_TESTS

}  // namespace google::cloud::odbc_tests
