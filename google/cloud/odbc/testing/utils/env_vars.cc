// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/internal/getenv.h"
#include <stdexcept>

namespace google::cloud::odbc_testing_utils {

std::string GetRequiredEnvVar(std::string const& var) {
  absl::optional<std::string> optional_env_var =
      ::google::cloud::internal::GetEnv(var.c_str());
  if (!optional_env_var) {
    throw std::runtime_error(var + " environment variable is not set");
  }
  if (optional_env_var->empty()) {
    throw std::runtime_error(var + " environment variable has an empty value");
  }
  return *optional_env_var;
}

}  // namespace google::cloud::odbc_testing_utils
