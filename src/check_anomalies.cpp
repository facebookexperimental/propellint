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

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <boost/program_options.hpp>

#include <clang/AST/ASTDumper.h>
#include <clang/AST/ASTTypeTraits.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/JSONCompilationDatabase.h>
#include <clang/Tooling/Tooling.h>

#include <omp.h>

#include <range/v3/all.hpp>

#include <simdjson.h>

#include <propellint/Buck.h>
#include <propellint/Matcher.h>
#include <propellint/Profile.h>

namespace fs = std::filesystem;
namespace json = simdjson;
namespace po = boost::program_options;

std::string toHumanReadable(uint64_t value) {
  static const std::vector<std::string> suffixes = {"", "k", "M", "G", "T"};

  auto index = 0;
  while (value >= 1000) {
    if (index == suffixes.size()) {
      break;
    }

    value /= 1000;
    ++index;
  }

  return std::to_string(value) + suffixes.at(index);
}

std::optional<std::pair<const char*, unsigned int>> getLocation(
    const clang::SourceLocation& location,
    const clang::SourceManager& SM) {
  if (!location.isValid() || !location.isFileID()) {
    return std::nullopt;
  }

  const auto presumed = SM.getPresumedLoc(location);
  if (!presumed.isValid()) {
    return std::nullopt;
  }

  return std::make_pair(presumed.getFilename(), presumed.getLine());
}

int main(int argc, char* argv[]) {
  po::options_description description("Options");
  // clang-format off
  description.add_options()
    ("help", "produce help message")
    ("profile", po::value<std::string>()->required(), "path to the JSON profile")
    ("directory", po::value<std::string>()->required(), "path to the source directory")
    ("jobs,j", po::value<size_t>()->default_value(1), "number of targets to process simultaneously");
  // clang-format on

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, description), vm);

  if (vm.count("help")) {
    std::cout << description << std::endl;
    return -1;
  }
  po::notify(vm);

  const auto profile = vm.at("profile").as<std::string>();
  const auto directory = vm.at("directory").as<std::string>();
  const auto jobs = vm.at("jobs").as<size_t>();

  std::cout << "Loading JSON file..." << std::endl;
  json::padded_string json = json::padded_string::load(profile);

  std::cout << "Parsing JSON file..." << std::endl;
  // We need the parser to have "global" scope so the string views stay valid.
  json::ondemand::parser parser;
  const auto insertOperatorBracketLocations =
      Profile::getInsertOperatorBracketLocations(parser, json);
  std::cout << "Successfully parsed JSON file ("
            << insertOperatorBracketLocations.size()
            << " total operator[] locations)." << std::endl;

  std::cout << "Finding compilation targets..." << std::endl;

  std::unordered_set<std::string> filenames;
  for (const auto& [site, _] : insertOperatorBracketLocations) {
    const auto filename = std::string(site.first);
    if (fs::exists(directory + "/" + filename)) {
      filenames.emplace(std::move(filename));
    }
  }

  const auto filenameToTargetMap =
      Buck::getFilenameToTargetMap(directory, filenames);
  std::unordered_map<std::string, std::unordered_set<Profile::CallSite>>
      targetToCallSitesMap;
  for (const auto& [site, _] : insertOperatorBracketLocations) {
    const auto it = filenameToTargetMap.find(std::string(site.first));
    // This can happen if no target was found for the given filename.
    if (it != filenameToTargetMap.end()) {
      for (const auto& target : it->second) {
        targetToCallSitesMap[target].insert(site);
      }
    }
  }
  std::cout << "Successfully found " << targetToCallSitesMap.size()
            << " targets." << std::endl;

  std::cout << "Building compilation database files..." << std::endl;
  auto targets = targetToCallSitesMap | ranges::views::keys |
      ranges::to<std::vector<std::string>>();
  const auto targetToDatabaseMap =
      Buck::buildCompilationDatabases2(directory, targets);
  std::cout << "Successfully built " << targetToDatabaseMap.size() << "/"
            << targetToCallSitesMap.size() << " compilation datases."
            << std::endl;

  // Filter out targets for which we couldn't get a compilation database.
  std::erase_if(targets, [&targetToDatabaseMap](const auto& entry) {
    return !targetToDatabaseMap.contains(entry);
  });

  const auto matcher = Matcher::get();

  omp_set_num_threads(jobs);
#pragma omp parallel for
  for (auto i = 0; i < targets.size(); ++i) {
    const auto& target = targets.at(i);
    const auto& sites = targetToCallSitesMap.at(target);
    const auto& database_path = targetToDatabaseMap.at(target);

    std::unordered_set<std::string> filenames;
    for (const auto& site : sites) {
      filenames.insert(directory + "/" + std::string(site.first));
    }

    std::string error;
    const auto database = clang::tooling::JSONCompilationDatabase::loadFromFile(
        database_path,
        error,
        clang::tooling::JSONCommandLineSyntax::AutoDetect);
    if (!database) {
      std::cerr << "Could not load compilation database for " << target << "."
                << std::endl
                << error << std::endl;
      continue;
    }

    clang::tooling::ClangTool tool(
        *database,
        std::vector<std::string>(filenames.begin(), filenames.end()));
    std::vector<std::unique_ptr<clang::ASTUnit>> ASTs;
    int status = tool.buildASTs(ASTs);
    assert(status == 0 || status == 1 || status == 2);
    if (status == 1) {
      std::cerr << "Failed to build ASTs for " << target << "." << std::endl;
      continue;
    } else if (status == 2) {
      std::cerr << "Failed to build an AST for some files in " << target << "."
                << std::endl;
    }

    for (size_t i = 0; i < ASTs.size(); ++i) {
      const auto matches =
          clang::ast_matchers::match(matcher, ASTs[i]->getASTContext());

      for (const auto& match : matches) {
        const auto* bracket =
            match.getNodeAs<clang::CXXOperatorCallExpr>("bracket");
        const auto location =
            getLocation(bracket->getExprLoc(), ASTs[i]->getSourceManager());
        if (!location.has_value()) {
          continue;
        }

        const auto& [filename, line] = location.value();
        const auto site = std::make_pair(std::string_view(filename), line);
        if (!sites.contains(site)) {
          continue;
        }

        const auto insertWeight = insertOperatorBracketLocations.at(site).first;
        const auto totalWeight = insertOperatorBracketLocations.at(site).second;
        std::cout << toHumanReadable(insertWeight) << "/"
                  << toHumanReadable(totalWeight) << " " << site.first << ":"
                  << site.second << std::endl;
      }
    }
  }
}
