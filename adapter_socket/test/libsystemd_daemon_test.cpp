#include "adapter_socket/systemd_daemon/libsystemd_daemon.h"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
#include <string_view>
#include <sys/socket.h>
#include <sys/un.h>
#include <system_error>
#include <unistd.h>
#include <vector>

using adapter_socket::LibsystemdDaemon;

constexpr const char* LISTEN_PID_ENVIRONMENT = "LISTEN_PID";
constexpr const char* LISTEN_FDS_ENVIRONMENT = "LISTEN_FDS";
constexpr int SYSCALL_ERROR = -1;
constexpr int DEFAULT_PROTOCOL = 0;
constexpr int LISTEN_BACKLOG = 4;
constexpr int UNSET_FD = -1;
constexpr std::size_t SOCKET_PAIR_FD_COUNT = 2;
constexpr std::size_t LOCAL_END = 0;
constexpr std::size_t PEER_END = 1;
constexpr std::size_t ABSTRACT_NAME_INDEX = 1;
constexpr std::string_view ABSTRACT_SOCKET_NAME = "adapter_socket_libsystemd_test";

class LibsystemdDaemonTest : public ::testing::Test
{
protected:
    [[noreturn]] static void throwSystemError(const char* operation)
    {
        const int savedErrno = errno;
        throw std::system_error(savedErrno, std::generic_category(), operation);
    }

    static int createAbstractListeningSocket()
    {
        const int listeningFd = ::socket(AF_UNIX, SOCK_STREAM, DEFAULT_PROTOCOL);
        if (listeningFd == SYSCALL_ERROR)
        {
            throwSystemError("socket");
        }

        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        std::memcpy(&address.sun_path[ABSTRACT_NAME_INDEX], ABSTRACT_SOCKET_NAME.data(),
                    ABSTRACT_SOCKET_NAME.size());
        const auto addressLength = static_cast<socklen_t>(
            offsetof(sockaddr_un, sun_path) + ABSTRACT_NAME_INDEX + ABSTRACT_SOCKET_NAME.size());
        const int bindResult =
            ::bind(listeningFd, reinterpret_cast<const sockaddr*>(&address), addressLength);
        const int listenResult =
            bindResult == SYSCALL_ERROR ? SYSCALL_ERROR : ::listen(listeningFd, LISTEN_BACKLOG);
        if (listenResult == SYSCALL_ERROR)
        {
            const int savedErrno = errno;
            ::close(listeningFd);
            throw std::system_error(savedErrno, std::generic_category(), "bind/listen");
        }
        return listeningFd;
    }

    void SetUp() override
    {
        ::unsetenv(LISTEN_PID_ENVIRONMENT);
        ::unsetenv(LISTEN_FDS_ENVIRONMENT);
    }

    const LibsystemdDaemon m_daemon;
};

TEST_F(LibsystemdDaemonTest, TakeListenFdsIsEmptyWhenNotSocketActivated)
{
    const adapter_socket::SyscallResult<std::vector<int>> listenFds = m_daemon.takeListenFds();

    ASSERT_FALSE(listenFds.hasFailed());
    EXPECT_TRUE(listenFds.value.empty());
}

TEST_F(LibsystemdDaemonTest, ListeningUnixStreamSocketMatches)
{
    const int listeningFd = createAbstractListeningSocket();

    const adapter_socket::SyscallResult<bool> isListening =
        m_daemon.isListeningUnixStream(listeningFd);
    ::close(listeningFd);

    ASSERT_FALSE(isListening.hasFailed());
    EXPECT_TRUE(isListening.value);
}

TEST_F(LibsystemdDaemonTest, ConnectedSocketIsNotAListeningMatch)
{
    std::array<int, SOCKET_PAIR_FD_COUNT> fds{UNSET_FD, UNSET_FD};
    const int pairResult = ::socketpair(AF_UNIX, SOCK_STREAM, DEFAULT_PROTOCOL, fds.data());
    if (pairResult == SYSCALL_ERROR)
    {
        throwSystemError("socketpair");
    }

    const adapter_socket::SyscallResult<bool> isListening =
        m_daemon.isListeningUnixStream(fds[LOCAL_END]);
    ::close(fds[LOCAL_END]);
    ::close(fds[PEER_END]);

    ASSERT_FALSE(isListening.hasFailed());
    EXPECT_FALSE(isListening.value);
}

TEST_F(LibsystemdDaemonTest, InvalidFdReportsNegatedErrno)
{
    const adapter_socket::SyscallResult<bool> isListening =
        m_daemon.isListeningUnixStream(UNSET_FD);

    ASSERT_TRUE(isListening.hasFailed());
    EXPECT_EQ(isListening.error, std::errc::bad_file_descriptor);
}
