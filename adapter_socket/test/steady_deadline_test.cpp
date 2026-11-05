#include "adapter_socket/deadline/steady_deadline.h"

#include "parameterized_case.h"

#include <chrono>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <vector>

using adapter_socket::SteadyDeadline;

constexpr std::chrono::milliseconds FUTURE_TIMEOUT(5000);
constexpr std::chrono::milliseconds ZERO_TIMEOUT(0);
constexpr std::chrono::milliseconds NEGATIVE_TIMEOUT(-100);
constexpr std::chrono::days OVERSIZED_TIMEOUT(30);
constexpr int NO_POLL_WAIT = 0;

TEST(SteadyDeadlineTest, FutureDeadlineHasNotExpired)
{
    const SteadyDeadline deadline(FUTURE_TIMEOUT);

    EXPECT_FALSE(deadline.hasExpired());
}

TEST(SteadyDeadlineTest, FutureDeadlinePollTimeoutIsPositiveAndWithinTimeout)
{
    const SteadyDeadline deadline(FUTURE_TIMEOUT);
    const int pollTimeout = deadline.pollTimeoutMilliseconds();

    EXPECT_GT(pollTimeout, NO_POLL_WAIT);
    EXPECT_LE(pollTimeout, FUTURE_TIMEOUT.count());
}

TEST(SteadyDeadlineTest, OversizedTimeoutPollTimeoutIsCappedAtIntMax)
{
    const SteadyDeadline deadline(OVERSIZED_TIMEOUT);

    EXPECT_FALSE(deadline.hasExpired());
    EXPECT_EQ(deadline.pollTimeoutMilliseconds(), std::numeric_limits<int>::max());
}

struct ExpiredTimeoutCase
{
    std::chrono::milliseconds timeout;
    const char* name;
};

const std::vector<ExpiredTimeoutCase> EXPIRED_TIMEOUTS{
    {ZERO_TIMEOUT, "ZeroTimeout"},
    {NEGATIVE_TIMEOUT, "NegativeTimeout"},
};

class SteadyDeadlineExpiredTest : public ::testing::TestWithParam<ExpiredTimeoutCase>
{
};

TEST_P(SteadyDeadlineExpiredTest, IsExpiredAndPollsNothing)
{
    const SteadyDeadline deadline(GetParam().timeout);

    EXPECT_TRUE(deadline.hasExpired());
    EXPECT_EQ(deadline.pollTimeoutMilliseconds(), NO_POLL_WAIT);
}

INSTANTIATE_TEST_SUITE_P(Timeout, SteadyDeadlineExpiredTest, ::testing::ValuesIn(EXPIRED_TIMEOUTS),
                         caseName<ExpiredTimeoutCase>);
