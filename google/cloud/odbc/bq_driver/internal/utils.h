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

#ifndef GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_UTILS_H
#define GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_UTILS_H

#include <iostream>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <memory>
#include <map>
#include <string>

namespace google {
namespace cloud {
namespace odbc_bq_driver {

using Section = std::map<std::string, std::string>;
using Sections = std::map<std::string, Section>;
//using Sections = std::map<std::string, std::shared_ptr<Section>>;

std::shared_ptr<Sections> ParseIni(std::string const& file_path);

}  // namespace odbc_bq_driver
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_UTILS_H