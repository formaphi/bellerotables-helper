#include "domain/use_case/redirection_problem.h"

#include "parameterized_case.h"

#include <gtest/gtest.h>
#include <vector>

using domain::RedirectionProblem;
using domain::RedirectionProblemKind;

struct RedirectionProblemEqualityCase
{
    RedirectionProblem left;
    RedirectionProblem right;
    bool expectedEqual;
    const char* name;
};

const std::vector<RedirectionProblemEqualityCase> EQUALITY_CASES{
    {{.kind = RedirectionProblemKind::KernelRejected, .detail = "EINVAL"},
     {.kind = RedirectionProblemKind::KernelRejected, .detail = "EINVAL"},
     true,
     "SameKindAndDetail"},
    {{.kind = RedirectionProblemKind::KernelRejected, .detail = "x"},
     {.kind = RedirectionProblemKind::TableUnavailable, .detail = "x"},
     false,
     "DifferentKind"},
    {{.kind = RedirectionProblemKind::Unexpected, .detail = "a"},
     {.kind = RedirectionProblemKind::Unexpected, .detail = "b"},
     false,
     "DifferentDetail"},
};

class RedirectionProblemEqualityTest
    : public ::testing::TestWithParam<RedirectionProblemEqualityCase>
{
};

TEST(RedirectionProblemTest, DefaultKindIsUnexpected)
{
    const RedirectionProblem problem;

    EXPECT_EQ(problem.kind, RedirectionProblemKind::Unexpected);
    EXPECT_TRUE(problem.detail.empty());
}

TEST_P(RedirectionProblemEqualityTest, ComparesKindAndDetail)
{
    const RedirectionProblemEqualityCase& testCase = GetParam();

    EXPECT_EQ(testCase.left == testCase.right, testCase.expectedEqual);
}

INSTANTIATE_TEST_SUITE_P(RedirectionProblem, RedirectionProblemEqualityTest,
                         ::testing::ValuesIn(EQUALITY_CASES),
                         caseName<RedirectionProblemEqualityCase>);
