#include <adapter_socket/deadline/steady_deadline.h>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <limits>

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

TEST(SteadyDeadlineTest, ZeroTimeoutIsExpiredAndPollsNothing)
{
    const SteadyDeadline deadline(ZERO_TIMEOUT);

    EXPECT_TRUE(deadline.hasExpired());
    EXPECT_EQ(deadline.pollTimeoutMilliseconds(), NO_POLL_WAIT);
}

TEST(SteadyDeadlineTest, NegativeTimeoutIsExpiredAndPollsNothing)
{
    const SteadyDeadline deadline(NEGATIVE_TIMEOUT);

    EXPECT_TRUE(deadline.hasExpired());
    EXPECT_EQ(deadline.pollTimeoutMilliseconds(), NO_POLL_WAIT);
}

TEST(SteadyDeadlineTest, OversizedTimeoutPollTimeoutIsCappedAtIntMax)
{
    const SteadyDeadline deadline(OVERSIZED_TIMEOUT);

    EXPECT_FALSE(deadline.hasExpired());
    EXPECT_EQ(deadline.pollTimeoutMilliseconds(), std::numeric_limits<int>::max());
}
