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


#include "utils.h"

#include "google/cloud/internal/getenv.h"

#include <gtest/gtest.h>

namespace google {
namespace cloud {
namespace odbc_bq_driver {

const Section DsnSection {
  { "Description", "Google BigQuery ODBC Connector" },
  { "Driver", "/opt/odbc-driver/googlebigqueryodbc/lib/libgooglebigqueryodbc_sb64.so" }
};

const Section OdbcSection {
  { "Trace", "1" },
  { "TraceFile", "/tmp/odbc.log" }
};

const Section commentedDsnSection {
  { "HTAPI_MinResultsSize", "1000" },
  { "HTAPI_MinActivationRatio", "3" }
};

Sections sampleIniSections {
  { "SampleDSN", DsnSection },
  { "ODBC", OdbcSection }
};

Sections commentedIniSections {
  { "SampleDSN", commentedDsnSection },
};

TEST(Parsing, ParseIni) {
  std::string test_data_path = google::cloud::internal::GetEnv("CPP_BIGQUERY_ODBC_DRIVER_TEST_DATA_PATH").value_or("");
  std::string path = std::filesystem::canonical(test_data_path + "/sample.ini");
  Sections sections = *ParseIni(path);
  for(auto it_outer = sampleIniSections.begin(); it_outer != sampleIniSections.end(); it_outer++ ) {
    std::string sectionName = it_outer->first;
    Section sampleIniSection = it_outer->second;
    for(auto it_inner = sampleIniSection.begin(); it_inner != sampleIniSection.end(); it_inner++ ) {
      std::string property = it_inner->first;
      EXPECT_EQ(sampleIniSection[property], sections[sectionName][property]);
    }
  }

  for(auto it_outer = commentedIniSections.begin(); it_outer != commentedIniSections.end(); it_outer++ ) {
    std::string sectionName = it_outer->first;
    Section commentIniSection = it_outer->second;
    for(auto it_inner = commentIniSection.begin(); it_inner != commentIniSection.end(); it_inner++ ) {
      std::string property = it_inner->first;
      EXPECT_EQ(sections[sectionName][property], "");
    }
  }

}

}  // namespace odbc_bq_driver
}  // namespace cloud
}  // namespace google
