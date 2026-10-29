#include <adapter_socket/socket_endpoint/socket_timeouts_dto.h>

#include <gtest/gtest.h>

#include <chrono>
#include <stdexcept>

constexpr std::chrono::milliseconds ACCEPT_TIMEOUT(250);
constexpr std::chrono::milliseconds CONNECTION_TIMEOUT(750);
constexpr std::chrono::milliseconds ZERO_TIMEOUT(0);
constexpr std::chrono::milliseconds NEGATIVE_TIMEOUT(-1);

TEST(SocketTimeoutsDtoTest, CreateKeepsPositiveTimeouts)
{
    const adapter_socket::SocketTimeoutsDto timeouts =
        adapter_socket::SocketTimeoutsDto::create(ACCEPT_TIMEOUT, CONNECTION_TIMEOUT);

    EXPECT_EQ(timeouts.acceptTimeout, ACCEPT_TIMEOUT);
    EXPECT_EQ(timeouts.connectionTimeout, CONNECTION_TIMEOUT);
}

TEST(SocketTimeoutsDtoTest, DefaultsArePositive)
{
    const adapter_socket::SocketTimeoutsDto timeouts;

    EXPECT_EQ(timeouts.acceptTimeout, adapter_socket::SocketTimeoutsDto::DEFAULT_ACCEPT_TIMEOUT);
    EXPECT_EQ(timeouts.connectionTimeout,
              adapter_socket::SocketTimeoutsDto::DEFAULT_CONNECTION_TIMEOUT);
    EXPECT_GT(timeouts.acceptTimeout, ZERO_TIMEOUT);
    EXPECT_GT(timeouts.connectionTimeout, ZERO_TIMEOUT);
}

TEST(SocketTimeoutsDtoTest, CreateRejectsZeroAcceptTimeout)
{
    EXPECT_THROW(static_cast<void>(
                     adapter_socket::SocketTimeoutsDto::create(ZERO_TIMEOUT, CONNECTION_TIMEOUT)),
                 std::invalid_argument);
}

TEST(SocketTimeoutsDtoTest, CreateRejectsZeroConnectionTimeout)
{
    EXPECT_THROW(
        static_cast<void>(adapter_socket::SocketTimeoutsDto::create(ACCEPT_TIMEOUT, ZERO_TIMEOUT)),
        std::invalid_argument);
}

TEST(SocketTimeoutsDtoTest, CreateRejectsNegativeTimeout)
{
    EXPECT_THROW(static_cast<void>(adapter_socket::SocketTimeoutsDto::create(NEGATIVE_TIMEOUT,
                                                                             CONNECTION_TIMEOUT)),
                 std::invalid_argument);
}
