#include "adapter_socket/descriptor_syscalls/posix_descriptor_syscalls.h"

#include "parameterized_case.h"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <vector>

using adapter_socket::PollOutcome;
using adapter_socket::PosixDescriptorSyscalls;
using adapter_socket::Readiness;

constexpr int SYSCALL_ERROR = -1;
constexpr int DEFAULT_PROTOCOL = 0;
constexpr int NO_FLAGS = 0;
constexpr int UNSET_FD = -1;
constexpr int FIRST_VALID_FD = 0;
constexpr std::size_t SOCKET_PAIR_FD_COUNT = 2;
constexpr std::size_t LOCAL_END = 0;
constexpr std::size_t PEER_END = 1;
constexpr std::size_t SINGLE_BYTE_COUNT = 1;
constexpr uint8_t SINGLE_PAYLOAD_BYTE = 0x5A;
constexpr int IMMEDIATE_POLL_TIMEOUT_MILLISECONDS = 0;
constexpr int READY_POLL_TIMEOUT_MILLISECONDS = 1000;

class PosixDescriptorSyscallsTest : public ::testing::Test
{
protected:
    static bool isFdClosed(int fd)
    {
        const int descriptorFlags = ::fcntl(fd, F_GETFD);
        const int savedErrno = errno;
        return descriptorFlags == SYSCALL_ERROR && savedErrno == EBADF;
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
        if (m_localFd >= FIRST_VALID_FD)
        {
            ::close(m_localFd);
        }
        ::close(m_peerFd);
    }

    int releaseLocalFd()
    {
        const int released = m_localFd;
        m_localFd = UNSET_FD;
        return released;
    }

    const PosixDescriptorSyscalls m_syscalls;
    int m_localFd = UNSET_FD;
    int m_peerFd = UNSET_FD;
};

TEST_F(PosixDescriptorSyscallsTest, CloseDescriptorClosesTheFd)
{
    const int localFd = releaseLocalFd();

    m_syscalls.closeDescriptor(localFd);

    EXPECT_TRUE(isFdClosed(localFd));
}

TEST_F(PosixDescriptorSyscallsTest, PollOnceWritableIsReadyOnFreshSocket)
{
    const adapter_socket::SyscallResult<PollOutcome> outcome =
        m_syscalls.pollOnce(m_localFd, Readiness::Writable, READY_POLL_TIMEOUT_MILLISECONDS);

    ASSERT_FALSE(outcome.hasFailed());
    EXPECT_EQ(outcome.value, PollOutcome::Ready);
}

TEST_F(PosixDescriptorSyscallsTest, EnableNonBlockingSetsTheStatusFlag)
{
    const std::error_code error = m_syscalls.enableNonBlocking(m_localFd);

    ASSERT_FALSE(error);
    const int flags = ::fcntl(m_localFd, F_GETFL);
    EXPECT_NE(flags & O_NONBLOCK, 0);
}

TEST_F(PosixDescriptorSyscallsTest, EnableNonBlockingReportsErrnoForClosedFd)
{
    const int closedFd = releaseLocalFd();
    ::close(closedFd);

    const std::error_code error = m_syscalls.enableNonBlocking(closedFd);

    EXPECT_EQ(error, std::errc::bad_file_descriptor);
}

const std::vector<ReadinessCase> INPUT_READINESS{
    {Readiness::Readable, "Readable"},
    {Readiness::Acceptable, "Acceptable"},
};

class PosixDescriptorSyscallsInputTest : public PosixDescriptorSyscallsTest,
                                         public ::testing::WithParamInterface<ReadinessCase>
{
};

TEST_P(PosixDescriptorSyscallsInputTest, PollOnceTimesOutWhenNothingArrived)
{
    const adapter_socket::SyscallResult<PollOutcome> outcome =
        m_syscalls.pollOnce(m_localFd, GetParam().readiness, IMMEDIATE_POLL_TIMEOUT_MILLISECONDS);

    ASSERT_FALSE(outcome.hasFailed());
    EXPECT_EQ(outcome.value, PollOutcome::TimedOut);
}

TEST_P(PosixDescriptorSyscallsInputTest, PollOnceIsReadyOncePeerWrote)
{
    const uint8_t payloadByte = SINGLE_PAYLOAD_BYTE;
    const ssize_t sent = ::send(m_peerFd, &payloadByte, SINGLE_BYTE_COUNT, NO_FLAGS);
    ASSERT_EQ(sent, static_cast<ssize_t>(SINGLE_BYTE_COUNT));

    const adapter_socket::SyscallResult<PollOutcome> outcome =
        m_syscalls.pollOnce(m_localFd, GetParam().readiness, READY_POLL_TIMEOUT_MILLISECONDS);

    ASSERT_FALSE(outcome.hasFailed());
    EXPECT_EQ(outcome.value, PollOutcome::Ready);
}

INSTANTIATE_TEST_SUITE_P(InputReadiness, PosixDescriptorSyscallsInputTest,
                         ::testing::ValuesIn(INPUT_READINESS), caseName<ReadinessCase>);
