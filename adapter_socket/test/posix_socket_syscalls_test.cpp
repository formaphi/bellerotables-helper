#include "adapter_socket/socket_syscalls/posix_socket_syscalls.h"

#include "adapter_socket/peer_connection/peer_credentials.h"
#include "adapter_socket/socket_syscalls/retryable_error.h"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <sys/socket.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <vector>

using adapter_socket::PeerCredentials;
using adapter_socket::PosixSocketSyscalls;

constexpr int SYSCALL_ERROR = -1;
constexpr int DEFAULT_PROTOCOL = 0;
constexpr int UNSET_FD = -1;
constexpr int FIRST_VALID_FD = 0;
constexpr std::size_t SOCKET_PAIR_FD_COUNT = 2;
constexpr std::size_t LOCAL_END = 0;
constexpr std::size_t PEER_END = 1;
constexpr std::size_t PEER_CLOSED = 0;
constexpr std::size_t RECEIVE_BUFFER_BYTES = 8;
const std::vector<uint8_t> PAYLOAD{1, 2, 3};
constexpr uint32_t INVALID_ID = std::numeric_limits<uint32_t>::max();
constexpr uint32_t ROOT_USER_ID = 0;
constexpr uint32_t NO_PEER_PROCESS_ID = 0;

class PosixSocketSyscallsTest : public ::testing::Test
{
protected:
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
        ::close(m_localFd);
        closePeer();
    }

    void closePeer()
    {
        if (m_peerFd >= FIRST_VALID_FD)
        {
            ::close(m_peerFd);
            m_peerFd = UNSET_FD;
        }
    }

    const PosixSocketSyscalls m_syscalls;
    int m_localFd = UNSET_FD;
    int m_peerFd = UNSET_FD;
};

TEST_F(PosixSocketSyscallsTest, SendSomeThenReceiveSomeRoundTripsBytes)
{
    const adapter_socket::SyscallResult<std::size_t> sent = m_syscalls.sendSome(m_peerFd, PAYLOAD);
    ASSERT_FALSE(sent.hasFailed());
    EXPECT_EQ(sent.value, PAYLOAD.size());

    std::vector<uint8_t> buffer(RECEIVE_BUFFER_BYTES);
    const adapter_socket::SyscallResult<std::size_t> received =
        m_syscalls.receiveSome(m_localFd, buffer);

    ASSERT_FALSE(received.hasFailed());
    buffer.resize(received.value);
    EXPECT_EQ(buffer, PAYLOAD);
}

TEST_F(PosixSocketSyscallsTest, ReceiveSomeDoesNotBlockWhenNothingArrived)
{
    std::vector<uint8_t> buffer(RECEIVE_BUFFER_BYTES);

    const adapter_socket::SyscallResult<std::size_t> received =
        m_syscalls.receiveSome(m_localFd, buffer);

    ASSERT_TRUE(received.hasFailed());
    EXPECT_TRUE(adapter_socket::isRetryableTransferError(received.error));
}

TEST_F(PosixSocketSyscallsTest, ReceiveSomeReturnsZeroAfterPeerClosed)
{
    closePeer();
    std::vector<uint8_t> buffer(RECEIVE_BUFFER_BYTES);

    const adapter_socket::SyscallResult<std::size_t> received =
        m_syscalls.receiveSome(m_localFd, buffer);

    ASSERT_FALSE(received.hasFailed());
    EXPECT_EQ(received.value, PEER_CLOSED);
}

TEST_F(PosixSocketSyscallsTest, SendSomeToClosedPeerReportsBrokenPipeInsteadOfSignal)
{
    closePeer();

    const adapter_socket::SyscallResult<std::size_t> sent = m_syscalls.sendSome(m_localFd, PAYLOAD);

    ASSERT_TRUE(sent.hasFailed());
    EXPECT_EQ(sent.error, std::errc::broken_pipe);
}

TEST_F(PosixSocketSyscallsTest, ReadPeerCredentialsReportsThisProcess)
{
    const adapter_socket::SyscallResult<PeerCredentials> peer =
        m_syscalls.readPeerCredentials(m_localFd);

    ASSERT_FALSE(peer.hasFailed());
    EXPECT_EQ(peer.value.userId, static_cast<uint32_t>(::getuid()));
    EXPECT_EQ(peer.value.groupId, static_cast<uint32_t>(::getgid()));
    EXPECT_EQ(peer.value.processId, static_cast<uint32_t>(::getpid()));
}

TEST_F(PosixSocketSyscallsTest, ReadPeerCredentialsWithoutPeerReportsInvalidIdsNotRoot)
{
    const int unconnectedFd = ::socket(AF_UNIX, SOCK_STREAM, DEFAULT_PROTOCOL);
    ASSERT_NE(unconnectedFd, SYSCALL_ERROR);

    const adapter_socket::SyscallResult<PeerCredentials> peer =
        m_syscalls.readPeerCredentials(unconnectedFd);
    ::close(unconnectedFd);

    ASSERT_FALSE(peer.hasFailed());
    EXPECT_EQ(peer.value.userId, INVALID_ID);
    EXPECT_EQ(peer.value.groupId, INVALID_ID);
    EXPECT_NE(peer.value.userId, ROOT_USER_ID);
    EXPECT_EQ(peer.value.processId, NO_PEER_PROCESS_ID);
}

TEST_F(PosixSocketSyscallsTest, AcceptConnectionOnNonListeningSocketReportsErrno)
{
    const adapter_socket::SyscallResult<int> accepted = m_syscalls.acceptConnection(m_localFd);

    ASSERT_TRUE(accepted.hasFailed());
    EXPECT_EQ(accepted.error, std::errc::invalid_argument);
}
