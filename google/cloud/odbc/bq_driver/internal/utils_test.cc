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

#include "google/cloud/internal/getenv.h"

#include <gtest/gtest.h>

namespace google {
namespace cloud {
namespace odbc_bq_driver {

const Section kDsnSection {
  { "Description", "Google BigQuery ODBC Connector" },
  { "Driver", "/opt/odbc-driver/googlebigqueryodbc/lib/libgooglebigqueryodbc_sb64.so" },
  { "PropertyWithoutValue", ""},
  { "Value with equals", "I=am=a=value"}
};

const Section kOdbcSection {
  { "Trace", "1" },
  { "TraceFile", "/tmp/odbc.log" }
};

const Section kCommentedDsnSection {
  { "HTAPI_MinResultsSize", "1000" },
  { "HTAPI_MinActivationRatio", "3" }
};

const Sections kSampleIniSections {
  { "SampleDSN", kDsnSection },
  { "ODBC", kOdbcSection }
};

const Sections kCommentedIniSections {
  { "SampleDSN", kCommentedDsnSection },
};

TEST(Parsing, ParseConfig) {
  std::string test_data_path = google::cloud::internal::GetEnv("CPP_BIGQUERY_ODBC_DRIVER_TEST_DATA_PATH").value_or("");
  auto sections = ParseConfig(test_data_path + "/sample.ini");

  // Test if the uncommented sections are defined
  for (const auto & it_outer : kSampleIniSections) {
    std::string section_name = it_outer.first;
    Section sample_ini_section = it_outer.second;
    for(auto & it_inner : sample_ini_section) {
      std::string property = it_inner.first;
      EXPECT_EQ(sample_ini_section[property], (*(sections.value()))[section_name][property]);
    }
  }

  // Test if the commented sections are not defined
  for (const auto & it_outer : kCommentedIniSections) {
    std::string section_name = it_outer.first;
    Section commented_ini_section = it_outer.second;
    for (auto & it_inner : commented_ini_section) {
      std::string property = it_inner.first;
      EXPECT_EQ((*(sections.value()))[section_name][property], "");
    }
  }
}

TEST(Parsing, ParseConfigIncorrectPath) {
  auto sections = ParseConfig("/invalid_file_name.ini");
  EXPECT_EQ(sections.status().code(), StatusCode::kInvalidArgument);
}

TEST(Parsing, ParseConnectionString) {
  Section testing_section = kDsnSection;
  std::string conn_str = "";
  // Create the connection string we will use for this test
  for (const auto & it : testing_section) {
    conn_str.append(it.first);
    conn_str.append("=");
    conn_str.append(it.second);
    conn_str.append(";");
  }

  Section section_ret = ParseConnectionString(conn_str);

  for (const auto & it : testing_section) {
    std::string field = it.first;
    std::string value = it.second;
    Trim(field);
    Trim(value);
    EXPECT_EQ(section_ret[field], value);
  }
}

}  // namespace odbc_bq_driver
}  // namespace cloud
}  // namespace google
