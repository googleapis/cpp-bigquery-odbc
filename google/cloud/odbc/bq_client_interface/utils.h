// Copyright 2025 Google LLC
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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_CLIENT_INTERFACE_UTILS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_CLIENT_INTERFACE_UTILS_H

#include <absl/log/log.h>
#include <string>
#include <thread>

template <typename Functor>
auto RetryLoop(Functor&& functor, std::string const& operation_name,
               int max_retries = 6, int initial_delay_ms = 500,
               int max_delay_ms = 20000,
               double backoff_multiplier = 2.0) -> decltype(functor()) {
  int attempt = 0;

  using ReturnType = decltype(functor());
  ReturnType response;

  google::cloud::ExponentialBackoffPolicy backoff_policy(
      std::chrono::milliseconds(initial_delay_ms),
      std::chrono::milliseconds(max_delay_ms), backoff_multiplier);

  while (attempt <= max_retries) {
    response = functor();

    if (response.ok()) {
      LOG(INFO) << operation_name << " succeeded on attempt " << attempt;
      return response;
    }

    auto code = response.status().code();
    std::string message = response.status().message();

    bool is_rate_limit = (code == google::cloud::StatusCode::kPermissionDenied);
        
    if ((code != google::cloud::StatusCode::kDeadlineExceeded &&
         !is_rate_limit)) {
      LOG(WARNING) << operation_name
                   << " failed permanently: " << response.status();
      return response;
    }

    auto delay = backoff_policy.OnCompletion();
    LOG(WARNING)
        << operation_name << " failed (attempt " << attempt
        << "): " << response.status() << " -- retrying after "
        << std::chrono::duration_cast<std::chrono::milliseconds>(delay).count()
        << "ms";

    std::this_thread::sleep_for(delay);
    ++attempt;
  }

  return response;
}

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_CLIENT_INTERFACE_UTILS_H
