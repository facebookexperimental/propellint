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

#include <clang/ASTMatchers/ASTMatchers.h>

using namespace clang::ast_matchers;

namespace {
auto isReferenceRightSideOfVarDecl() {
  return hasAncestor(varDecl(hasType(lValueReferenceType())));
}

auto isLHSOfAssignment() {
  const auto hasLHSOperatorBracket = hasLHS(expr(anyOf(
      equalsBoundNode("bracket"),
      hasDescendant(expr(equalsBoundNode("bracket"))))));

  return anyOf(
      hasAncestor(
          cxxOperatorCallExpr(isAssignmentOperator(), hasLHSOperatorBracket)),
      hasAncestor(
          binaryOperator(isAssignmentOperator(), hasLHSOperatorBracket)));
}

auto isPartOfIncrementOrDecrementExpr() {
  return anyOf(
      hasAncestor(unaryOperator(anyOf(hasOperatorName("++"), hasOperatorName("--")))),
      hasAncestor(cxxOperatorCallExpr(anyOf(hasOperatorName("++"), hasOperatorName("--")))));
}
} // namespace

namespace Matcher {
const auto get() {
  return traverse(
      clang::TK_IgnoreUnlessSpelledInSource,
      cxxOperatorCallExpr(
          cxxOperatorCallExpr().bind("bracket"),
          hasOverloadedOperatorName("[]"),
          unless(anyOf(
              isReferenceRightSideOfVarDecl(),
              isLHSOfAssignment(),
              isPartOfIncrementOrDecrementExpr()))));
}
} // namespace Matcher
