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

#include "propellint/Profile.h"

bool isOperatorBracket(std::string_view entry) {
  return std::unordered_set<std::string_view>(
             {
                 "std::map::operator[]",
                 "std::unordered_map::operator[]",
                 "folly::sorted_vector_map::operator[]",
                 "folly::f14::detail::F14BasicMap::operator[]",
                 "facebook::multifeed::QuickHashMap::operator[]",
                 "facebook::datastruct::FBHashMap::operator[]",
             })
      .contains(entry);
}

std::size_t getOperatorBracketIndex(
    const std::vector<Profile::StackEntry>& stack) {
  for (int i = 0; i < stack.size(); ++i) {
    if (isOperatorBracket(stack[i].function)) {
      return i;
    }
  }

  return -1;
}

bool isInsertStack(
    const std::vector<Profile::StackEntry>& stack,
    std::size_t index) {
  assert(getOperatorBracketIndex(stack) == index);
  const auto function = stack[index].function;

  if (function == "std::map::operator[]") {
    return std::find_if(
               stack.begin() + index + 1, stack.end(), [](const auto& entry) {
                 return entry.function ==
                     "std::_Rb_tree::_M_emplace_hint_unique";
               }) != stack.end();
  }

  if (function == "std::unordered_map::operator[]") {
    return std::find_if(
               stack.begin() + index + 1, stack.end(), [](const auto& entry) {
                 return std::unordered_set<std::string_view>(
                            {"std::_Hashtable::_Scoped_node::_Scoped_node",
                             "std::_Hashtable::_M_insert_unique_node"})
                     .contains(entry.function);
               }) != stack.end();
  }

  if (function == "folly::sorted_vector_map::operator[]") {
    return std::find_if(
               stack.begin() + index + 1, stack.end(), [](const auto& entry) {
                 return entry.function == "folly::sorted_vector_map::insert";
               }) != stack.end();
  }

  if (function == "folly::f14::detail::F14BasicMap::operator[]") {
    return std::find_if(
               stack.begin() + index + 1, stack.end(), [](const auto& entry) {
                 return std::unordered_set<std::string_view>(
                            {"folly::f14::detail::F14Table::reserveForInsert",
                             "folly::f14::detail::F14Table::insertAtBlank"})
                     .contains(entry.function);
               }) != stack.end();
  }

  if (function == "facebook::multifeed::QuickHashMap::operator[]") {
    return std::find_if(stack.begin() + index + 1, stack.end(), [](const auto& entry) {
             return std::unordered_set<std::string_view>(
                        {"facebook::multifeed::detail::QuickHashTable::growSize",
                         "facebook::multifeed::detail::QuickHashTable::EntryImpl::EntryImpl"})
                 .contains(entry.function);
           }) != stack.end();
  }

  if (function == "facebook::datastruct::FBHashMap::operator[]") {
    return std::find_if(
               stack.begin() + index + 1, stack.end(), [](const auto& entry) {
                 return entry.function ==
                     "facebook::datastruct::FBHashMap::append_";
               }) != stack.end();
  }

  throw "This should never happen.";
}

// No false-positives, minimal false-negatives.
bool isInsertStack(const std::vector<Profile::StackEntry>& stack) {
  const auto index = getOperatorBracketIndex(stack);
  assert(index != -1);
  return isInsertStack(stack, index);
}

Profile::StackEntry parseProfileEntry(std::string_view entry) {
  const auto i = entry.find("@");
  const auto j = entry.find(":", i + 1);

  const auto fn = entry.substr(0, i);
  const auto filename = entry.substr(i + 1, j - i - 1);

  int line = -1;
  if (j != std::string_view::npos) {
    line = std::stoi(std::string(entry.substr(j + 1)));
  }

  return {fn, filename, line};
}

std::unordered_map<Profile::CallSite, std::pair<uint64_t, uint64_t>>
Profile::getOperatorBracketLocations(
    json::ondemand::parser& parser,
    const json::padded_string& json) {
  json::ondemand::document profile = parser.iterate(json);
  std::unordered_map<Profile::CallSite, std::pair<uint64_t, uint64_t>>
      operatorBracketLocations;

  for (auto profileEntry : profile.get_array()) {
    std::vector<Profile::StackEntry> stack;
    for (std::string_view entry : profileEntry["stack_combined"]) {
      stack.push_back(parseProfileEntry(entry));
    }

    // Remove thrift indirection.
    std::erase_if(stack, [](const auto& entry) {
      return entry.function == "apache::thrift::field_ref::operator[]";
    });

    const auto i = getOperatorBracketIndex(stack);
    if (i == 0 || i == -1) {
      continue;
    }
    const auto caller = stack[i - 1];
    const CallSite location(caller.filename, caller.line);

    if (isInsertStack(stack, i)) {
      operatorBracketLocations[location].first +=
          uint64_t(profileEntry["total_weight"]);
    }

    operatorBracketLocations[location].second +=
        uint64_t(profileEntry["total_weight"]);
  }

  return operatorBracketLocations;
}

// We are not interested in operator[] calls that never insert.
std::unordered_map<Profile::CallSite, std::pair<uint64_t, uint64_t>>
Profile::getInsertOperatorBracketLocations(
    json::ondemand::parser& parser,
    const json::padded_string& json) {
  auto locations = Profile::getOperatorBracketLocations(parser, json);
  std::erase_if(locations, [](const auto& location) {
    return location.second.first == 0;
  });

  return locations;
}
