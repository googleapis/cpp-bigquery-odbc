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

namespace google {
namespace cloud {
namespace odbc_bq_driver {

inline void LTrim(std::string& s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(),
    [](char ch) { return (std::isspace(ch) == 0); }));
}

inline void RTrim(std::string& s) {
	s.erase(std::find_if(s.rbegin(), s.rend(), [](char ch) { return (std::isspace(ch) == 0); }).base(), s.end());
}

inline void Trim(std::string& s) {
  LTrim(s);
  RTrim(s);
}

std::shared_ptr<Sections> ParseIni(std::string const& file_path) {
  std::ifstream is(file_path);
  is.exceptions(std::ios::badbit);  // Minimal error handling
  Sections sections;
  if (is.is_open()) {
    std::string line;
    std::string current_section_name;
    while (getline(is, line)) {
      Trim(line);
      if (line.empty() || line.at(0) == ';' || line.at(0) == '#') {
        // Blank lines and comment lines are ignored.
      } else if (line.at(0) == '[' && line.back() == ']') {
        // Section line.
        line.erase(0, 1);
        line.pop_back();
        Trim(line);
        current_section_name = line;
      } else {
        // Property line.
        size_t pos = line.find_first_of('=');
        std::string property;
        std::string value;
        if (pos != std::string::npos) {
          property = line.substr(0, pos);
          Trim(property);
          value = line.substr(pos + 1);
          Trim(value);
        } else {
          property = line;
          Trim(property);
          value = "";
        }
        if(!current_section_name.empty()) {
          sections[current_section_name][property] = value;
        }
      }
    }
  } else {
    throw std::runtime_error("Cannot access file");
  }
  return std::make_shared<Sections>(sections);
}

}  // namespace odbc_bq_driver
}  // namespace cloud
}  // namespace google
