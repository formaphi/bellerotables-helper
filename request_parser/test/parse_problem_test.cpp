#include "request_parser/parser/parse_problem.h"

#include "parameterized_case.h"

#include <gtest/gtest.h>
#include <vector>

using request_parser::ParseProblem;
using request_parser::ParseProblemKind;

struct ParseProblemEqualityCase
{
    ParseProblem left;
    ParseProblem right;
    bool expectedEqual;
    const char* name;
};

const std::vector<ParseProblemEqualityCase> EQUALITY_CASES{
    {{.kind = ParseProblemKind::ContractMismatch, .detail = "unknown field"},
     {.kind = ParseProblemKind::ContractMismatch, .detail = "unknown field"},
     true,
     "SameKindAndDetail"},
    {{.kind = ParseProblemKind::MalformedJson, .detail = "x"},
     {.kind = ParseProblemKind::ConstraintViolation, .detail = "x"},
     false,
     "DifferentKind"},
    {{.kind = ParseProblemKind::ConstraintViolation, .detail = "a"},
     {.kind = ParseProblemKind::ConstraintViolation, .detail = "b"},
     false,
     "DifferentDetail"},
};

class ParseProblemEqualityTest : public ::testing::TestWithParam<ParseProblemEqualityCase>
{
};

TEST_P(ParseProblemEqualityTest, ComparesKindAndDetail)
{
    const ParseProblemEqualityCase& testCase = GetParam();

    EXPECT_EQ(testCase.left == testCase.right, testCase.expectedEqual);
}

INSTANTIATE_TEST_SUITE_P(ParseProblem, ParseProblemEqualityTest,
                         ::testing::ValuesIn(EQUALITY_CASES), caseName<ParseProblemEqualityCase>);
