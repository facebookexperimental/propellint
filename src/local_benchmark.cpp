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

#include <unistd.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <unordered_map>

static std::ofstream out;

namespace intentional {
std::map<int, int> histogram(const std::vector<int>& values) {
  std::map<int, int> hist;
  for (const auto& value : values) {
    ++hist[value];
  }
  return hist;
}

std::unordered_map<int, std::vector<int>> initialize(
    const std::vector<std::pair<int, int>>& sizes) {
  std::unordered_map<int, std::vector<int>> result;
  for (const auto& [id, size] : sizes) {
    auto& v = result[id];
    if (v.empty()) {
      v = {size, id};
    } else {
      std::vector<int> merge(size, id);
      v.insert(v.end(), merge.begin(), merge.end());
    }
  }
  return result;
}
} // namespace intentional

namespace unintentional {
void iterate(std::unordered_map<int, std::vector<int>>& map) {
  for (int i = 0; i < 100'000; ++i) {
    for (const auto& value : map[i]) {
      out << value;
    }
  }
}

void print(std::unordered_map<int, int>& map) {
  for (int i = 0; i < 100'000; ++i) {
    out << map[i];
  }
}
} // namespace unintentional

void printElapsedTime(
    std::chrono::high_resolution_clock::time_point start,
    std::chrono::high_resolution_clock::time_point end) {
  const auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
          .count() /
      1.0e3;
  std::cout << "Time elapsed: " << duration << " second." << std::endl;
}

int main() {
  std::minstd_rand generator(std::random_device{}());

  std::cout << "Running on PID " << getpid() << "." << std::endl;

  for (auto i = 0; true; ++i) {
    std::cout << "Iteration #" << i + 1 << "." << std::endl;

    {
      std::vector<int> values;
      values.reserve(1'000'000);
      std::uniform_int_distribution<int> distribution;
      for (int i = 0; i < 1'000'000; ++i) {
        values.emplace_back(distribution(generator));
      }
      out << intentional::histogram(values).size();
    }

    {
      std::vector<std::pair<int, int>> sizes;
      sizes.reserve(1'000'000);
      std::uniform_int_distribution<int> idDistribution(10'000);
      std::uniform_int_distribution<int> sizeDistribution(10);
      for (int i = 0; i < 1'000'000; ++i) {
        sizes.emplace_back(
            idDistribution(generator), sizeDistribution(generator));
      }
      out << intentional::initialize(sizes).size();
    }

    {
      std::unordered_map<int, int> map;
      map.reserve(1'000'000);
      std::uniform_int_distribution<int> distribution;
      for (int i = 0; i < 1'000'000; ++i) {
        int value = distribution(generator);
        map.emplace(value, value);
      }
      unintentional::print(map);
    }

    {
      std::unordered_map<int, std::vector<int>> map;
      map.reserve(1'000'000);
      std::uniform_int_distribution<int> keyDistribution;
      std::uniform_int_distribution<int> sizeDistribution(0, 10);
      for (int i = 0; i < 1'000'000; ++i) {
        map.emplace(keyDistribution(generator), sizeDistribution(generator));
      }
      unintentional::iterate(map);
    }
  }
}
