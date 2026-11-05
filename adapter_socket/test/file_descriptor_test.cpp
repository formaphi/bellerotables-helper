#include "adapter_socket/file_descriptor/file_descriptor.h"

#include "adapter_socket/descriptor_syscalls/interface_descriptor_syscalls.h"
#include "adapter_socket/descriptor_syscalls/posix_descriptor_syscalls.h"
#include "adapter_socket/mock/mock_deadline.h"
#include "adapter_socket/mock/mock_descriptor_syscalls.h"
#include "parameterized_case.h"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <vector>

using adapter_socket::IDescriptorSyscalls;
using adapter_socket::MockDeadline;
using adapter_socket::MockDescriptorSyscalls;
using adapter_socket::PollOutcome;
using adapter_socket::PosixDescriptorSyscalls;
using adapter_socket::Readiness;
using adapter_socket::SyscallResult;
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::InSequence;
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
constexpr int FAKE_FD = 4242;
constexpr int NO_ERROR_CAUGHT = 0;
constexpr int FAKE_POLL_TIMEOUT_MILLISECONDS = 250;
constexpr const char* POLL_OPERATION = "poll";

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
    std::shared_ptr<IDescriptorSyscalls> m_syscalls = std::make_shared<PosixDescriptorSyscalls>();
};

TEST_F(FileDescriptorTest, ValueReturnsWrappedDescriptor)
{
    const adapter_socket::FileDescriptor owned(m_localFd, m_syscalls);

    EXPECT_EQ(owned.value(), m_localFd);
}

TEST_F(FileDescriptorTest, DestructorClosesWrappedDescriptor)
{
    {
        const adapter_socket::FileDescriptor owned(m_localFd, m_syscalls);
    }

    EXPECT_TRUE(isFdClosed(m_localFd));
}

TEST_F(FileDescriptorTest, NegativeDescriptorIsNotClosed)
{
    {
        const adapter_socket::FileDescriptor unset(NEGATIVE_FD, m_syscalls);

        EXPECT_EQ(unset.value(), NEGATIVE_FD);
    }

    EXPECT_FALSE(isFdClosed(m_localFd));
    ::close(m_localFd);
}

TEST_F(FileDescriptorTest, WaitUntilReadyReadableReturnsOncePeerWrote)
{
    const adapter_socket::FileDescriptor owned(m_localFd, m_syscalls);
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
    const adapter_socket::FileDescriptor owned(m_localFd, m_syscalls);

    MockDeadline pendingDeadline;
    EXPECT_CALL(pendingDeadline, hasExpired()).WillRepeatedly(Return(DEADLINE_PENDING));
    EXPECT_CALL(pendingDeadline, pollTimeoutMilliseconds())
        .WillRepeatedly(Return(READY_POLL_TIMEOUT_MILLISECONDS));
    EXPECT_NO_THROW(owned.waitUntilReady(adapter_socket::Readiness::Writable, pendingDeadline));
}

const std::vector<ReadinessCase> ALL_READINESS{
    {Readiness::Readable, "Readable"},
    {Readiness::Writable, "Writable"},
    {Readiness::Acceptable, "Acceptable"},
};

class FileDescriptorExpiredDeadlineTest : public FileDescriptorTest,
                                          public ::testing::WithParamInterface<ReadinessCase>
{
};

TEST_P(FileDescriptorExpiredDeadlineTest, WaitUntilReadyThrowsTimedOutWithoutPolling)
{
    const adapter_socket::FileDescriptor owned(m_localFd, m_syscalls);

    expectDeadlineExceeded(owned, GetParam().readiness, DEADLINE_EXCEEDED_MESSAGE);
}

INSTANTIATE_TEST_SUITE_P(AllReadiness, FileDescriptorExpiredDeadlineTest,
                         ::testing::ValuesIn(ALL_READINESS), caseName<ReadinessCase>);

class FileDescriptorSyscallsTest : public ::testing::Test
{
protected:
    static SyscallResult<PollOutcome> pollFailure(std::errc error)
    {
        return SyscallResult<PollOutcome>::failure(std::make_error_code(error));
    }

    static SyscallResult<PollOutcome> pollOutcome(PollOutcome outcome)
    {
        return SyscallResult<PollOutcome>::success(outcome);
    }

    std::shared_ptr<MockDescriptorSyscalls> m_syscalls = std::make_shared<MockDescriptorSyscalls>();
    MockDeadline m_pendingDeadline;
};

TEST_F(FileDescriptorSyscallsTest, DestructorClosesThroughAdapter)
{
    EXPECT_CALL(*m_syscalls, closeDescriptor(FAKE_FD)).Times(1);

    const adapter_socket::FileDescriptor owned(FAKE_FD, m_syscalls);
}

TEST_F(FileDescriptorSyscallsTest, NegativeDescriptorIsNeverClosedThroughAdapter)
{
    EXPECT_CALL(*m_syscalls, closeDescriptor(_)).Times(0);

    const adapter_socket::FileDescriptor unset(NEGATIVE_FD, m_syscalls);
}

TEST_F(FileDescriptorSyscallsTest, WaitUntilReadyPollsOwnFdWithRequestedReadinessAndTimeout)
{
    EXPECT_CALL(*m_syscalls, closeDescriptor(FAKE_FD));
    EXPECT_CALL(m_pendingDeadline, hasExpired()).WillOnce(Return(DEADLINE_PENDING));
    EXPECT_CALL(m_pendingDeadline, pollTimeoutMilliseconds())
        .WillOnce(Return(FAKE_POLL_TIMEOUT_MILLISECONDS));
    EXPECT_CALL(*m_syscalls, pollOnce(FAKE_FD, Readiness::Writable, FAKE_POLL_TIMEOUT_MILLISECONDS))
        .WillOnce(Return(pollOutcome(PollOutcome::Ready)));
    const adapter_socket::FileDescriptor owned(FAKE_FD, m_syscalls);

    EXPECT_NO_THROW(owned.waitUntilReady(Readiness::Writable, m_pendingDeadline));
}

TEST_F(FileDescriptorSyscallsTest, WaitUntilReadyRetriesWhenPollIsInterrupted)
{
    EXPECT_CALL(*m_syscalls, closeDescriptor(FAKE_FD));
    EXPECT_CALL(m_pendingDeadline, hasExpired()).WillRepeatedly(Return(DEADLINE_PENDING));
    EXPECT_CALL(m_pendingDeadline, pollTimeoutMilliseconds())
        .WillRepeatedly(Return(FAKE_POLL_TIMEOUT_MILLISECONDS));
    {
        const InSequence inOrder;
        EXPECT_CALL(*m_syscalls, pollOnce(FAKE_FD, Readiness::Readable, _))
            .WillOnce(Return(pollFailure(std::errc::interrupted)));
        EXPECT_CALL(*m_syscalls, pollOnce(FAKE_FD, Readiness::Readable, _))
            .WillOnce(Return(pollOutcome(PollOutcome::Ready)));
    }
    const adapter_socket::FileDescriptor owned(FAKE_FD, m_syscalls);

    EXPECT_NO_THROW(owned.waitUntilReady(Readiness::Readable, m_pendingDeadline));
}

TEST_F(FileDescriptorSyscallsTest, WaitUntilReadyRechecksDeadlineAfterPollTimesOut)
{
    EXPECT_CALL(*m_syscalls, closeDescriptor(FAKE_FD));
    EXPECT_CALL(m_pendingDeadline, hasExpired())
        .WillOnce(Return(DEADLINE_PENDING))
        .WillOnce(Return(DEADLINE_EXPIRED));
    EXPECT_CALL(m_pendingDeadline, pollTimeoutMilliseconds())
        .WillOnce(Return(FAKE_POLL_TIMEOUT_MILLISECONDS));
    EXPECT_CALL(*m_syscalls, pollOnce(FAKE_FD, Readiness::Readable, _))
        .WillOnce(Return(pollOutcome(PollOutcome::TimedOut)));
    const adapter_socket::FileDescriptor owned(FAKE_FD, m_syscalls);

    int errorValue = NO_ERROR_CAUGHT;
    try
    {
        owned.waitUntilReady(Readiness::Readable, m_pendingDeadline);
    }
    catch (const std::system_error& error)
    {
        errorValue = error.code().value();
    }
    EXPECT_EQ(errorValue, ETIMEDOUT);
}

TEST_F(FileDescriptorSyscallsTest, WaitUntilReadyThrowsNonRetryablePollError)
{
    EXPECT_CALL(*m_syscalls, closeDescriptor(FAKE_FD));
    EXPECT_CALL(m_pendingDeadline, hasExpired()).WillOnce(Return(DEADLINE_PENDING));
    EXPECT_CALL(m_pendingDeadline, pollTimeoutMilliseconds())
        .WillOnce(Return(FAKE_POLL_TIMEOUT_MILLISECONDS));
    EXPECT_CALL(*m_syscalls, pollOnce(FAKE_FD, Readiness::Readable, _))
        .WillOnce(Return(pollFailure(std::errc::bad_file_descriptor)));
    const adapter_socket::FileDescriptor owned(FAKE_FD, m_syscalls);

    int errorValue = NO_ERROR_CAUGHT;
    std::string message;
    try
    {
        owned.waitUntilReady(Readiness::Readable, m_pendingDeadline);
    }
    catch (const std::system_error& error)
    {
        errorValue = error.code().value();
        message = error.what();
    }
    EXPECT_EQ(errorValue, EBADF);
    EXPECT_THAT(message, HasSubstr(POLL_OPERATION));
}
