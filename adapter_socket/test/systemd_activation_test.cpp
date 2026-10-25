#include <adapter_socket/activation/systemd_activation.h>

#include <adapter_socket/exceptions/socket_exception.h>
#include <adapter_socket/file_descriptor/interface_file_descriptor.h>

#include <gtest/gtest.h>
#include <systemd/sd-daemon.h>

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>

constexpr const char* LISTEN_PID_ENVIRONMENT = "LISTEN_PID";
constexpr const char* LISTEN_FDS_ENVIRONMENT = "LISTEN_FDS";
constexpr const char* SINGLE_ACTIVATED_FD_TEXT = "1";
constexpr int OVERWRITE_ENVIRONMENT = 1;
constexpr int SYSCALL_ERROR = -1;
constexpr int DEFAULT_PROTOCOL = 0;
constexpr int NO_SAVED_FD = -1;
constexpr int LISTEN_BACKLOG = 4;
constexpr std::size_t SOCKET_PAIR_FD_COUNT = 2;
constexpr std::size_t LOCAL_END = 0;
constexpr std::size_t PEER_END = 1;
constexpr std::size_t ABSTRACT_NAME_INDEX = 1;
constexpr std::string_view ABSTRACT_SOCKET_NAME = "adapter_socket_activation_test";

class SystemdActivationTest : public ::testing::Test
{
protected:
    [[noreturn]] static void throwSystemError(const char* operation)
    {
        const int savedErrno = errno;
        throw std::system_error(savedErrno, std::generic_category(), operation);
    }

    static void closeUnlessActivationFd(int fd)
    {
        if (fd != SD_LISTEN_FDS_START)
        {
            ::close(fd);
        }
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
        if (bindResult == SYSCALL_ERROR)
        {
            const int savedErrno = errno;
            ::close(listeningFd);
            throw std::system_error(savedErrno, std::generic_category(), "bind");
        }

        const int listenResult = ::listen(listeningFd, LISTEN_BACKLOG);
        if (listenResult == SYSCALL_ERROR)
        {
            const int savedErrno = errno;
            ::close(listeningFd);
            throw std::system_error(savedErrno, std::generic_category(), "listen");
        }
        return listeningFd;
    }

    static void installActivationFd(int sourceFd)
    {
        const int duplicateResult = ::dup2(sourceFd, SD_LISTEN_FDS_START);
        if (duplicateResult == SYSCALL_ERROR)
        {
            throwSystemError("dup2");
        }
        closeUnlessActivationFd(sourceFd);
    }

    static void setActivationEnvironment()
    {
        const std::string processId = std::to_string(::getpid());
        const int pidResult =
            ::setenv(LISTEN_PID_ENVIRONMENT, processId.c_str(), OVERWRITE_ENVIRONMENT);
        if (pidResult == SYSCALL_ERROR)
        {
            throwSystemError("setenv LISTEN_PID");
        }

        const int fdsResult =
            ::setenv(LISTEN_FDS_ENVIRONMENT, SINGLE_ACTIVATED_FD_TEXT, OVERWRITE_ENVIRONMENT);
        if (fdsResult == SYSCALL_ERROR)
        {
            throwSystemError("setenv LISTEN_FDS");
        }
    }

    void SetUp() override
    {
        ::unsetenv(LISTEN_PID_ENVIRONMENT);
        ::unsetenv(LISTEN_FDS_ENVIRONMENT);

        const int duplicatedFd = ::dup(SD_LISTEN_FDS_START);
        m_savedActivationFd = duplicatedFd == SYSCALL_ERROR ? NO_SAVED_FD : duplicatedFd;
    }

    void TearDown() override
    {
        ::unsetenv(LISTEN_PID_ENVIRONMENT);
        ::unsetenv(LISTEN_FDS_ENVIRONMENT);

        if (m_savedActivationFd == NO_SAVED_FD)
        {
            ::close(SD_LISTEN_FDS_START);
            return;
        }

        ::dup2(m_savedActivationFd, SD_LISTEN_FDS_START);
        ::close(m_savedActivationFd);
        m_savedActivationFd = NO_SAVED_FD;
    }

    const adapter_socket::SystemdActivation m_activation{};
    int m_savedActivationFd = NO_SAVED_FD;
};

TEST_F(SystemdActivationTest, ThrowsWhenProcessWasNotSocketActivated)
{
    EXPECT_THROW(static_cast<void>(m_activation.takeListeningFd()),
                 adapter_socket::ActivationException);
}

TEST_F(SystemdActivationTest, ReturnsActivatedListeningFdAndUnsetsActivationEnvironment)
{
    const int listeningFd = createAbstractListeningSocket();
    installActivationFd(listeningFd);
    setActivationEnvironment();

    const std::unique_ptr<adapter_socket::IFileDescriptor> activatedFd =
        m_activation.takeListeningFd();

    EXPECT_EQ(activatedFd->value(), SD_LISTEN_FDS_START);
    EXPECT_EQ(::getenv(LISTEN_PID_ENVIRONMENT), nullptr);
    EXPECT_EQ(::getenv(LISTEN_FDS_ENVIRONMENT), nullptr);
}

TEST_F(SystemdActivationTest, TakenListeningFdIsNonBlocking)
{
    const int listeningFd = createAbstractListeningSocket();
    installActivationFd(listeningFd);
    setActivationEnvironment();

    const std::unique_ptr<adapter_socket::IFileDescriptor> activatedFd =
        m_activation.takeListeningFd();
    const int flags = ::fcntl(activatedFd->value(), F_GETFL);
    const bool isNonBlocking = (flags & O_NONBLOCK) != 0;

    EXPECT_TRUE(isNonBlocking);
}

TEST_F(SystemdActivationTest, RejectsActivatedFdThatIsNotListening)
{
    std::array<int, SOCKET_PAIR_FD_COUNT> fds{};
    const int pairResult = ::socketpair(AF_UNIX, SOCK_STREAM, DEFAULT_PROTOCOL, fds.data());
    if (pairResult == SYSCALL_ERROR)
    {
        throwSystemError("socketpair");
    }

    installActivationFd(fds[LOCAL_END]);
    closeUnlessActivationFd(fds[PEER_END]);
    setActivationEnvironment();

    EXPECT_THROW(static_cast<void>(m_activation.takeListeningFd()),
                 adapter_socket::ActivationException);
}
