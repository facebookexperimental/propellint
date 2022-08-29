# Copyright (c) Meta Platforms, Inc. and affiliates.

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.19)

include(FetchContent)

project(PropelLint)

find_package(Boost 1.60 COMPONENTS program_options REQUIRED)
find_package(Clang REQUIRED CONFIG)

message(STATUS "Using LLVM/Clang version ${LLVM_PACKAGE_VERSION}.")

FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG        c4ee726532178e556d923372f29163bd206d7732 # 9.0.0
)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        58d77fa8070e8cec2dc1ed015d66b454c8d78850 # release-1.12.1
)
FetchContent_Declare(
  range-v3
  GIT_REPOSITORY https://github.com/ericniebler/range-v3.git
  GIT_TAG        a81477931a8aa2ad025c6bda0609f38e09e4d7ec # 0.12.0
)
FetchContent_Declare(
  simdjson
  GIT_REPOSITORY https://github.com/simdjson/simdjson.git
  GIT_TAG        933c2ebeacf9a1df12acd9a6781e590cad2f81f7 # v2.2.2
)
FetchContent_MakeAvailable(fmt googletest range-v3 simdjson)

include_directories(SYSTEM ${CLANG_INCLUDE_DIRS})
include_directories(include)

add_executable(propellint src/check_anomalies.cpp src/Buck.cpp src/Profile.cpp)
set_property(TARGET propellint PROPERTY CXX_STANDARD 20)
target_link_libraries(
  propellint
  Boost::program_options
  clangAST clangASTMatchers clangFrontend clangTooling
  fmt
  range-v3
  simdjson
)

add_executable(benchmark src/local_benchmark.cpp)
set_property(TARGET benchmark PROPERTY CXX_STANDARD 20)

enable_testing()

add_executable(MatcherTest test/MatcherTest.cpp)
target_link_libraries(
  MatcherTest
  clangASTMatchers clangTooling
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(MatcherTest)
