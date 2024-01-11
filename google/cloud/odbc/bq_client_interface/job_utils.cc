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
#include "google/cloud/bigquery/v2/minimal/internal/job_request.h"

namespace google::cloud::odbc_bigquery_client_interface {

StatusOr<StateFilter> GetStateFilterFromString(std::string const& str) {
  if ("PENDING" == str) {
    return StateFilter::kPending;
  }
  if ("RUNNING" == str) {
    return StateFilter::kRunning;
  }
  if ("DONE" == str) {
    return StateFilter::kDone;
  }
  if (str.empty()) {
    return StateFilter::kUninitialized;
  }
  return Status(StatusCode::kInvalidArgument,
                "An error happened while converting a string to StateFilter. "
                "No StateFilter for a string " +
                    str);
}

StatusOr<Projection> GetProjectionFromString(std::string const& str) {
  if ("MINIMAL" == str) {
    return Projection::kMinimal;
  }
  if ("FULL" == str) {
    return Projection::kFull;
  }
  if (str.empty()) {
    return Projection::kUninitialized;
  }
  return Status(StatusCode::kInvalidArgument,
                "An error happened while converting a string to Projection. "
                "No Projection for a string " +
                    str);
}

::google::cloud::bigquery_v2_minimal_internal::StateFilter convert(
    StateFilter state_filter) {
  switch (state_filter) {
    case StateFilter::kPending:
      return ::google::cloud::bigquery_v2_minimal_internal::StateFilter::
          Pending();
    case StateFilter::kRunning:
      return ::google::cloud::bigquery_v2_minimal_internal::StateFilter::
          Running();
    case StateFilter::kDone:
      return ::google::cloud::bigquery_v2_minimal_internal::StateFilter::Done();
    default:
      ::google::cloud::bigquery_v2_minimal_internal::StateFilter
          empty_state_filter;
      return empty_state_filter;
  }
}

::google::cloud::bigquery_v2_minimal_internal::Projection convert(
    Projection projection) {
  switch (projection) {
    case Projection::kMinimal:
      return ::google::cloud::bigquery_v2_minimal_internal::Projection::
          Minimal();
    case Projection::kFull:
      return ::google::cloud::bigquery_v2_minimal_internal::Projection::Full();
    default:
      ::google::cloud::bigquery_v2_minimal_internal::Projection
          empty_projection;
      return empty_projection;
  }
}

}  // namespace google::cloud::odbc_bigquery_client_interface
