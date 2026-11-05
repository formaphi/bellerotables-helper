#include <adapter_socket/deadline/steady_deadline_factory.h>
#include <chrono>
#include <gtest/gtest.h>
#include <memory>

using adapter_socket::IDeadline;
using adapter_socket::SteadyDeadlineFactory;

constexpr std::chrono::milliseconds FUTURE_TIMEOUT(5000);

TEST(SteadyDeadlineFactoryTest, StartingNowReturnsUnexpiredDeadline)
{
    const SteadyDeadlineFactory factory;

    const std::unique_ptr<IDeadline> deadline = factory.startingNow(FUTURE_TIMEOUT);

    ASSERT_NE(deadline.get(), nullptr);
    EXPECT_FALSE(deadline->hasExpired());
}
