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

#include <fstream>
#include <vector>

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Tooling/Tooling.h>

#include <gtest/gtest.h>

#include <propellint/Matcher.h>

static const std::string kMockMapCode = R"(
  namespace std {
  template<class Key, class T>
  struct map {
    T& operator[](const Key&);
    T& operator[](Key&&);
  };
  } // namespace std
)";

TEST(Matcher, testReferenceRightSideOfVarDecl) {
  const auto code = R"(
    void f() {
      std::map<int, int> map;
      auto& value = map[0];
    }
  )";

  std::unique_ptr<clang::ASTUnit> AST =
      clang::tooling::buildASTFromCode(kMockMapCode + code);
  assert(AST != nullptr);

  const auto matches =
      clang::ast_matchers::match(Matcher::get(), AST->getASTContext());
  EXPECT_EQ(matches.size(), 0);
}

TEST(Matcher, testRightSideOfVarDecl) {
  const auto code = R"(
    void f() {
      std::map<int, int> map;
      const auto value = map[0];
    }
  )";

  const auto AST = clang::tooling::buildASTFromCode(kMockMapCode + code);
  assert(AST != nullptr);

  const auto matches =
      clang::ast_matchers::match(Matcher::get(), AST->getASTContext());
  EXPECT_EQ(matches.size(), 1);
}

TEST(Matcher, testRightSideOfAssignment) {
  const auto code = R"(
    void f() {
      std::map<int, int> map;
      int value;
      value = map[0];
    }
  )";

  const auto AST = clang::tooling::buildASTFromCode(kMockMapCode + code);
  assert(AST != nullptr);

  const auto matches =
      clang::ast_matchers::match(Matcher::get(), AST->getASTContext());
  EXPECT_EQ(matches.size(), 1);
}

TEST(Matcher, testLHSOfAssignment) {
  const auto code = R"(
    void f() {
      std::map<int, int> map;
      map[0] = 1;
    }
  )";

  const auto AST = clang::tooling::buildASTFromCode(kMockMapCode + code);
  assert(AST != nullptr);

  const auto matches =
      clang::ast_matchers::match(Matcher::get(), AST->getASTContext());
  EXPECT_EQ(matches.size(), 0);
}

TEST(Matcher, testLHSOfAssignmentWithObject) {
  const auto code = R"(
    struct S {};
    void f() {
      std::map<int, S> map;
      map[0] = S();
    }
  )";

  const auto AST = clang::tooling::buildASTFromCode(kMockMapCode + code);
  assert(AST != nullptr);

  const auto matches =
      clang::ast_matchers::match(Matcher::get(), AST->getASTContext());
  EXPECT_EQ(matches.size(), 0);
}

TEST(Matcher, testPreIncrement) {
  const auto code = R"(
    void f() {
      std::map<int, int> map;
      ++map[0];
    }
  )";

  const auto AST = clang::tooling::buildASTFromCode(kMockMapCode + code);
  assert(AST != nullptr);

  const auto matches =
      clang::ast_matchers::match(Matcher::get(), AST->getASTContext());
  EXPECT_EQ(matches.size(), 0);
}

TEST(Matcher, testPreDecrement) {
  const auto code = R"(
    void f() {
      std::map<int, int> map;
      --map[0];
    }
  )";

  const auto AST = clang::tooling::buildASTFromCode(kMockMapCode + code);
  assert(AST != nullptr);

  const auto matches =
      clang::ast_matchers::match(Matcher::get(), AST->getASTContext());
  EXPECT_EQ(matches.size(), 0);
}

TEST(Matcher, testPostIncrement) {
  const auto code = R"(
    void f() {
      std::map<int, int> map;
      map[0]++;
    }
  )";

  const auto AST = clang::tooling::buildASTFromCode(kMockMapCode + code);
  assert(AST != nullptr);

  const auto matches =
      clang::ast_matchers::match(Matcher::get(), AST->getASTContext());
  EXPECT_EQ(matches.size(), 0);
}

TEST(Matcher, testPreIncrementWithObject) {
  const auto code = R"(
    struct S {
      S& operator++();
    };
    void f() {
      std::map<int, S> map;
      ++map[0];
    }
  )";

  const auto AST = clang::tooling::buildASTFromCode(kMockMapCode + code);
  assert(AST != nullptr);

  const auto matches =
      clang::ast_matchers::match(Matcher::get(), AST->getASTContext());
  EXPECT_EQ(matches.size(), 0);
}
