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

#pragma once

// A utility header to process a profile.
// This tool requires a profile following the following JSON structure:
// [{stack: ["function@filename:line", ...], total_weight: number}, ...]

#include <algorithm>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <boost/functional/hash.hpp>

#include <simdjson.h>

namespace json = simdjson;

namespace Profile {
struct StackEntry {
  StackEntry(std::string_view function, std::string_view filename, int line)
      : function(function), filename(filename), line(line) {}

  friend std::ostream& operator<<(std::ostream& out, const StackEntry& entry) {
    return out << entry.function << "@" << entry.filename << ":" << entry.line;
  }

  bool operator==(const StackEntry& other) const {
    return function == other.function && filename == other.filename &&
        line == other.line;
  }

  std::string_view function;
  std::string_view filename;
  int line;
};

using CallSite = std::pair<std::string_view, int>;

// Extracts all operator[] locations and their weight from a JSON profile.
// This returns two weights: a lower bound on the relative time spent inserting,
// and the total weight.
std::unordered_map<Profile::CallSite, std::pair<uint64_t, uint64_t>>
getOperatorBracketLocations(
    json::ondemand::parser& parser,
    const json::padded_string& json);

std::unordered_map<CallSite, std::pair<uint64_t, uint64_t>>
getInsertOperatorBracketLocations(
    json::ondemand::parser& parser,
    const json::padded_string& json);
} // namespace Profile

namespace std {
template <>
struct hash<Profile::StackEntry> {
  size_t operator()(const Profile::StackEntry& entry) const noexcept {
    size_t seed = 0;
    boost::hash_combine(seed, std::hash<std::string_view>()(entry.function));
    boost::hash_combine(seed, std::hash<std::string_view>()(entry.filename));
    boost::hash_combine(seed, entry.line);
    return seed;
  }
};

template <>
struct hash<Profile::CallSite> {
  std::size_t operator()(const Profile::CallSite& pair) const noexcept {
    std::size_t seed = 0;
    boost::hash_combine(seed, std::hash<std::string_view>()(pair.first));
    boost::hash_combine(seed, std::hash<int>()(pair.second));
    return seed;
  }
};
} // namespace std
