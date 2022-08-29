// Copyright (c) Meta Platforms, Inc. and affiliates.

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "propellint/Buck.h"

namespace json = simdjson;

// Utility. Joins strings by putting a delimiter between each entry.
template <typename Container>
std::string join(Container container, std::string delimiter) {
  const auto first = std::begin(container);
  const auto last = std::end(container);
  const auto distance = std::distance(first, last);
  if (distance == 0) {
    return "";
  }

  auto it = first;
  std::ostringstream result;
  result << *it;
  while (++it != last) {
    result << delimiter << *it;
  }

  return result.str();
}

// Utility. Runs command and returns exit code and standard output.
std::pair<int, std::string> check_output(const std::string& command) {
  auto* stdout = popen(command.c_str(), "r");
  assert(stdout != nullptr);
  char chunk[256];
  std::string output;
  while (fgets(chunk, sizeof(chunk), stdout) != nullptr) {
    output.append(chunk);
  }

  const auto status = pclose(stdout);
  assert(status != -1);

  return {status, output};
}

std::unordered_map<std::string, std::vector<std::string>>
Buck::getFilenameToTargetMap(
    const std::string directory,
    const std::unordered_set<std::string>& filenames) {
  const auto* filenames_filename = tmpnam(nullptr);
  assert(filenames_filename != nullptr);
  std::ofstream filenames_file(filenames_filename);
  for (const auto& filename : filenames) {
    filenames_file << filename << std::endl;
  }
  filenames_file.close();

  const auto [status, output] = check_output(fmt::format(
      R"(cd {}; buck1 query --json 'owner("%s")' @{})",
      directory,
      filenames_filename));
  assert(status == 0);

  json::ondemand::parser parser;
  json::padded_string json(output);
  json::ondemand::document document = parser.iterate(json);
  std::unordered_map<std::string, std::vector<std::string>> filenameToTargetMap;
  for (auto entry : document.get_object()) {
    const auto key = std::string(std::string_view(entry.unescaped_key()));
    for (auto target : entry.value().get_array()) {
      filenameToTargetMap[key].push_back(std::string(std::string_view(target)));
    }
  }
  return filenameToTargetMap;
}

// There is a more efficient "batch" way to do this which arc lint does.
// "Broken" targets need to be filtered out first.
std::unordered_map<std::string, std::string> Buck::buildCompilationDatabases(
    const std::string directory,
    const std::vector<std::string>& targets) {
  std::unordered_map<std::string, std::string> targetToDatabaseMap;
  for (const auto& target : targets) {
    const auto database_target = target + "#compilation-database";
    const auto command = fmt::format(
        "cd {}; buck1 build --show-full-json-output {} 2>/dev/null",
        directory,
        database_target);
    std::cout << "Building database for " << target << "..." << std::endl;
    const auto [status, output] = check_output(command);

    if (status != 0) {
      std::cout << "Could not build database for " << target << std::endl;
    } else {
      json::ondemand::parser parser;
      json::padded_string json(output);
      json::ondemand::document document = parser.iterate(json);
      for (auto entry : document.get_object()) {
        const auto key = std::string_view(entry.unescaped_key());
        targetToDatabaseMap.emplace(
            key.substr(0, key.size() - strlen("#compilation-database")),
            std::string_view(entry.value()));
      }
    }
  }

  return targetToDatabaseMap;
}

// A "hack" faster version -- remove "broken" targets by hand.
// This is obviously not optimal, but speeds this up a lot.
std::unordered_map<std::string, std::string> Buck::buildCompilationDatabases2(
    const std::string directory,
    const std::vector<std::string>& targets) {
  // Add broken targets below.
  static const auto broken = std::unordered_set<std::string>({});

  const auto* targets_filename = tmpnam(nullptr);
  assert(targets_filename != nullptr);
  std::ofstream targets_file(targets_filename);
  for (const auto& target : targets) {
    if (!broken.contains(target)) {
      const auto database_target = target + "#compilation-database";
      targets_file << database_target << std::endl;
    }
  }
  targets_file.close();
  const auto [status, output] = check_output(fmt::format(
      "cd {}; buck1 build @{} --show-full-json-output",
      directory,
      targets_filename));
  assert(status == 0);

  std::unordered_map<std::string, std::string> targetToDatabaseMap;
  json::ondemand::parser parser;
  json::padded_string json(output);
  json::ondemand::document document = parser.iterate(json);
  for (auto entry : document.get_object()) {
    const auto key = std::string_view(entry.unescaped_key());
    targetToDatabaseMap.emplace(
        key.substr(0, key.size() - strlen("#compilation-database")),
        std::string_view(entry.value()));
  }
  return targetToDatabaseMap;
}
