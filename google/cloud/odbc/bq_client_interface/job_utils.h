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

#ifndef GOOGLE_CLOUD_ODBC_BQ_DRIVER_CLIENT_INTERFACE_BQ_ENUMS_H
#define GOOGLE_CLOUD_ODBC_BQ_DRIVER_CLIENT_INTERFACE_BQ_ENUMS_H

#include "google/cloud/bigquery/v2/minimal/internal/job_request.h"
#include "google/cloud/status_or.h"
#include <chrono>

namespace google::cloud::odbc_bigquery_client_interface {

enum class StateFilter { kPending, kRunning, kDone, kUninitialized };

enum class Projection { kMinimal, kFull, kUninitialized };

// Filters used for filtering a list of Jobs.
// returned in Job response.
struct JobFilter {
  // Whether to include jobs by all users.
  bool allUsers = false;
  // Minimum point in time for job creation time.
  std::chrono::system_clock::time_point min_creation_time;
  // Maximum point in time for job creation time.
  std::chrono::system_clock::time_point max_creation_time;
  // Filtering by Job state: DONE, PENDING or RUNNING.
  StateFilter state_filter = StateFilter::kUninitialized;
  // Filters to return the child job of a specific parent.
  std::string parent_job_id;
  // Filtering based on specific Job fields: MINIMAL or FULL.
  Projection projection = Projection::kUninitialized;
};

StatusOr<StateFilter> GetStateFilterFromString(std::string const& str);
StatusOr<Projection> GetProjectionFromString(std::string const& str);

::google::cloud::bigquery_v2_minimal_internal::StateFilter convert(
    StateFilter state_filter);
::google::cloud::bigquery_v2_minimal_internal::Projection convert(
    Projection projection);

}  // namespace google::cloud::odbc_bigquery_client_interface

#endif  // GOOGLE_CLOUD_ODBC_BQ_DRIVER_CLIENT_INTERFACE_BQ_ENUMS_H
