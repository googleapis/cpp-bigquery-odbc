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

#include "google/cloud/odbc/bq_client_interface/job_utils.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include <gmock/gmock.h>

namespace google::cloud::odbc_bigquery_client_interface {

using google::cloud::odbc_testing_utils::StatusIs;
using ::testing::HasSubstr;

TEST(GetStateFilterFromString, Pending) {
  StatusOr<StateFilter> state_filter = GetStateFilterFromString("PENDING");

  EXPECT_EQ(StateFilter::kPending, *state_filter);
}

TEST(GetStateFilterFromString, Running) {
  StatusOr<StateFilter> state_filter = GetStateFilterFromString("RUNNING");

  EXPECT_EQ(StateFilter::kRunning, *state_filter);
}

TEST(GetStateFilterFromString, Done) {
  StatusOr<StateFilter> state_filter = GetStateFilterFromString("DONE");

  EXPECT_EQ(StateFilter::kDone, *state_filter);
}

TEST(GetStateFilterFromString, Empty) {
  StatusOr<StateFilter> state_filter = GetStateFilterFromString("");

  EXPECT_EQ(StateFilter::kUninitialized, *state_filter);
}

TEST(GetStateFilterFromString, RandomString) {
  StatusOr<StateFilter> state_filter =
      GetStateFilterFromString("not-valid-string");

  EXPECT_THAT(state_filter, StatusIs(StatusCode::kInvalidArgument,
                                     HasSubstr("No StateFilter for a string")));
}

TEST(GetProjectionFromString, Minimal) {
  StatusOr<Projection> projection = GetProjectionFromString("MINIMAL");

  EXPECT_EQ(Projection::kMinimal, *projection);
}

TEST(GetProjectionFromString, Full) {
  StatusOr<Projection> projection = GetProjectionFromString("FULL");

  EXPECT_EQ(Projection::kFull, *projection);
}

TEST(GetProjectionFromString, Empty) {
  StatusOr<Projection> projection = GetProjectionFromString("");

  EXPECT_EQ(Projection::kUninitialized, *projection);
}

TEST(GetProjectionFromString, RandomString) {
  StatusOr<Projection> projection = GetProjectionFromString("not-valid-string");

  EXPECT_THAT(projection, StatusIs(StatusCode::kInvalidArgument,
                                   HasSubstr("No Projection for a string")));
}

TEST(ConvertStateFilter, Pending) {
  ::google::cloud::bigquery_v2_minimal_internal::StateFilter state_filter =
      convert(StateFilter::kPending);

  EXPECT_EQ(
      state_filter.value,
      ::google::cloud::bigquery_v2_minimal_internal::StateFilter::Pending()
          .value);
}

TEST(ConvertStateFilter, Running) {
  ::google::cloud::bigquery_v2_minimal_internal::StateFilter state_filter =
      convert(StateFilter::kRunning);

  EXPECT_EQ(
      state_filter.value,
      ::google::cloud::bigquery_v2_minimal_internal::StateFilter::Running()
          .value);
}

TEST(ConvertStateFilter, Done) {
  ::google::cloud::bigquery_v2_minimal_internal::StateFilter state_filter =
      convert(StateFilter::kDone);

  EXPECT_EQ(
      state_filter.value,
      ::google::cloud::bigquery_v2_minimal_internal::StateFilter::Done().value);
}

TEST(ConvertStateFilter, Uninitialized) {
  ::google::cloud::bigquery_v2_minimal_internal::StateFilter state_filter =
      convert(StateFilter::kUninitialized);

  EXPECT_EQ(state_filter.value, "");
}

TEST(ConvertProjection, Minimal) {
  ::google::cloud::bigquery_v2_minimal_internal::Projection projection =
      convert(Projection::kMinimal);

  EXPECT_EQ(projection.value,
            ::google::cloud::bigquery_v2_minimal_internal::Projection::Minimal()
                .value);
}

TEST(ConvertProjection, Full) {
  ::google::cloud::bigquery_v2_minimal_internal::Projection projection =
      convert(Projection::kFull);

  EXPECT_EQ(
      projection.value,
      ::google::cloud::bigquery_v2_minimal_internal::Projection::Full().value);
}

TEST(ConvertProjection, Uninitialized) {
  ::google::cloud::bigquery_v2_minimal_internal::Projection projection =
      convert(Projection::kUninitialized);

  EXPECT_EQ(projection.value, "");
}

}  // namespace google::cloud::odbc_bigquery_client_interface
