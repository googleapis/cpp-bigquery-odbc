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

#include "google/cloud/odbc/bq_client_interface/utils.h"
#include <gtest/gtest.h>
#include <string>

namespace google::cloud::odbc_bigquery_client_interface {
namespace {

TEST(ParsePartnerToken, ValidFull) {
  std::string raw = "(GPN:PartnerName; Environment)";
  auto result = ParsePartnerToken(raw);
  EXPECT_TRUE(result.Ok());
  EXPECT_EQ(*result, " (GPN:PartnerName; Environment)");
}

TEST(ParsePartnerToken, ValidShort) {
  std::string raw = "(GPN:PartnerName)";
  auto result = ParsePartnerToken(raw);
  EXPECT_TRUE(result.Ok());
  EXPECT_EQ(*result, " (GPN:PartnerName)");
}

TEST(ParsePartnerToken, ValidWithSpaces) {
  std::string raw = " ( GPN:PartnerName ; Environment ) ";
  auto result = ParsePartnerToken(raw);
  EXPECT_TRUE(result.Ok());
  EXPECT_EQ(*result, " (GPN:PartnerName; Environment)");
}

TEST(ParsePartnerToken, InvalidNoGpn) {
  std::string raw = "(PartnerName; Environment)";
  auto result = ParsePartnerToken(raw);
  EXPECT_FALSE(result.Ok());
}

TEST(ParsePartnerToken, Empty) {
  std::string raw;
  auto result = ParsePartnerToken(raw);
  EXPECT_TRUE(result.Ok());
  EXPECT_EQ(*result, "");
}

TEST(ParsePartnerToken, NoMatch) {
  std::string raw = "invalid";
  auto result = ParsePartnerToken(raw);
  EXPECT_FALSE(result.Ok());
}

}  // namespace
}  // namespace google::cloud::odbc_bigquery_client_interface
