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

#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <fmt/format.h>

#include <simdjson.h>

namespace Buck {
std::unordered_map<std::string, std::vector<std::string>>
getFilenameToTargetMap(
    const std::string directory,
    const std::unordered_set<std::string>& filenames);
std::unordered_map<std::string, std::string> buildCompilationDatabases(
    const std::string directory,
    const std::vector<std::string>& targets);
std::unordered_map<std::string, std::string> buildCompilationDatabases2(
    const std::string directory,
    const std::vector<std::string>& targets);
} // namespace Buck
