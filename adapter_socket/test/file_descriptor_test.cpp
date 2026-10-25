#include <adapter_socket/file_descriptor/file_descriptor.h>

#include <adapter_socket/mock/mock_deadline.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <string>
#include <system_error>

using adapter_socket::MockDeadline;
using ::testing::HasSubstr;
using ::testing::Return;

constexpr int SYSCALL_ERROR = -1;
constexpr int SYSCALL_SUCCESS = 0;
constexpr int DEFAULT_PROTOCOL = 0;
constexpr int NO_FLAGS = 0;
constexpr int UNSET_FD = -1;
constexpr int NEGATIVE_FD = -7;
constexpr std::size_t LOCAL_END = 0;
constexpr std::size_t PEER_END = 1;
constexpr std::size_t SOCKET_PAIR_FD_COUNT = 2;
constexpr std::size_t SINGLE_BYTE_COUNT = 1;
constexpr uint8_t SINGLE_PAYLOAD_BYTE = 0x5A;
constexpr int READY_POLL_TIMEOUT_MILLISECONDS = 1000;
constexpr bool DEADLINE_EXPIRED = true;
constexpr bool DEADLINE_PENDING = false;
constexpr const char* DEADLINE_EXCEEDED_MESSAGE = "poll deadline exceeded";

class FileDescriptorTest : public ::testing::Test
{
protected:
    static bool isFdClosed(int fd)
    {
        const int descriptorFlags = ::fcntl(fd, F_GETFD);
        const int savedErrno = errno;
        return descriptorFlags == SYSCALL_ERROR && savedErrno == EBADF;
    }

    static void expectDeadlineExceeded(const adapter_socket::FileDescriptor& waiter,
                                       adapter_socket::Readiness readiness,
                                       const char* expectedMessage)
    {
        MockDeadline expiredDeadline;
        EXPECT_CALL(expiredDeadline, hasExpired()).WillOnce(Return(DEADLINE_EXPIRED));
        EXPECT_CALL(expiredDeadline, pollTimeoutMilliseconds()).Times(0);
        std::string message;
        try
        {
            waiter.waitUntilReady(readiness, expiredDeadline);
            ADD_FAILURE() << "waitUntilReady returned without readiness";
        }
        catch (const std::system_error& error)
        {
            EXPECT_EQ(error.code().value(), ETIMEDOUT);
            message = error.what();
        }
        EXPECT_THAT(message, HasSubstr(expectedMessage));
    }

    void SetUp() override
    {
        std::array<int, SOCKET_PAIR_FD_COUNT> fds{};
        const int pairResult = ::socketpair(AF_UNIX, SOCK_STREAM, DEFAULT_PROTOCOL, fds.data());
        if (pairResult == SYSCALL_ERROR)
        {
            const int savedErrno = errno;
            throw std::system_error(savedErrno, std::generic_category(), "socketpair");
        }

        m_localFd = fds[LOCAL_END];
        m_peerFd = fds[PEER_END];
    }

    void TearDown() override
    {
        if (m_peerFd != UNSET_FD)
        {
            ::close(m_peerFd);
        }
    }

    int m_localFd = UNSET_FD;
    int m_peerFd = UNSET_FD;
};

TEST_F(FileDescriptorTest, ValueReturnsWrappedDescriptor)
{
    const adapter_socket::FileDescriptor owned(m_localFd);

    EXPECT_EQ(owned.value(), m_localFd);
}

TEST_F(FileDescriptorTest, DestructorClosesWrappedDescriptor)
{
    {
        const adapter_socket::FileDescriptor owned(m_localFd);
    }

    EXPECT_TRUE(isFdClosed(m_localFd));
}

TEST_F(FileDescriptorTest, NegativeDescriptorIsNotClosed)
{
    {
        const adapter_socket::FileDescriptor unset(NEGATIVE_FD);

        EXPECT_EQ(unset.value(), NEGATIVE_FD);
    }

    EXPECT_FALSE(isFdClosed(m_localFd));
    ::close(m_localFd);
}

TEST_F(FileDescriptorTest, WaitUntilReadyReadableReturnsOncePeerWrote)
{
    const adapter_socket::FileDescriptor owned(m_localFd);
    const uint8_t payloadByte = SINGLE_PAYLOAD_BYTE;
    const ssize_t sent = ::send(m_peerFd, &payloadByte, SINGLE_BYTE_COUNT, NO_FLAGS);
    EXPECT_EQ(sent, static_cast<ssize_t>(SINGLE_BYTE_COUNT));

    MockDeadline pendingDeadline;
    EXPECT_CALL(pendingDeadline, hasExpired()).WillRepeatedly(Return(DEADLINE_PENDING));
    EXPECT_CALL(pendingDeadline, pollTimeoutMilliseconds())
        .WillRepeatedly(Return(READY_POLL_TIMEOUT_MILLISECONDS));
    EXPECT_NO_THROW(owned.waitUntilReady(adapter_socket::Readiness::Readable, pendingDeadline));
}

TEST_F(FileDescriptorTest, WaitUntilReadyWritableReturnsOnFreshSocket)
{
    const adapter_socket::FileDescriptor owned(m_localFd);

    MockDeadline pendingDeadline;
    EXPECT_CALL(pendingDeadline, hasExpired()).WillRepeatedly(Return(DEADLINE_PENDING));
    EXPECT_CALL(pendingDeadline, pollTimeoutMilliseconds())
        .WillRepeatedly(Return(READY_POLL_TIMEOUT_MILLISECONDS));
    EXPECT_NO_THROW(owned.waitUntilReady(adapter_socket::Readiness::Writable, pendingDeadline));
}

TEST_F(FileDescriptorTest, WaitUntilReadyReadableTimesOutWithReadDeadlineMessage)
{
    const adapter_socket::FileDescriptor owned(m_localFd);

    expectDeadlineExceeded(owned, adapter_socket::Readiness::Readable, DEADLINE_EXCEEDED_MESSAGE);
}

TEST_F(FileDescriptorTest, WaitUntilReadyAcceptableTimesOutWithAcceptDeadlineMessage)
{
    const adapter_socket::FileDescriptor owned(m_localFd);

    expectDeadlineExceeded(owned, adapter_socket::Readiness::Acceptable, DEADLINE_EXCEEDED_MESSAGE);
}
