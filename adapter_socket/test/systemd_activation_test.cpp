#include "adapter_socket/activation/systemd_activation.h"

#include "adapter_socket/descriptor_syscalls/posix_descriptor_syscalls.h"
#include "adapter_socket/exceptions/socket_exception.h"
#include "adapter_socket/file_descriptor/interface_file_descriptor.h"
#include "adapter_socket/mock/mock_descriptor_syscalls.h"
#include "adapter_socket/mock/mock_systemd_daemon.h"
#include "adapter_socket/systemd_daemon/libsystemd_daemon.h"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <system_error>
#include <systemd/sd-daemon.h>
#include <unistd.h>
#include <utility>
#include <vector>

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
constexpr int FAKE_ACTIVATED_FD = 4243;
constexpr int SECOND_FAKE_ACTIVATED_FD = 4244;
constexpr int NO_ERROR_CAUGHT = 0;
constexpr const char* MULTIPLE_SOCKETS_MESSAGE = "expected exactly one activated socket, got 2";

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

    const adapter_socket::SystemdActivation m_activation{
        std::make_unique<adapter_socket::LibsystemdDaemon>(),
        std::make_shared<adapter_socket::PosixDescriptorSyscalls>()};
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

using adapter_socket::MockDescriptorSyscalls;
using adapter_socket::MockSystemdDaemon;
using adapter_socket::SyscallResult;
using ::testing::_;
using ::testing::Return;

class SystemdActivationSyscallsTest : public ::testing::Test
{
protected:
    static std::error_code failure(std::errc error)
    {
        return std::make_error_code(error);
    }

    static SyscallResult<std::vector<int>> activated(const std::vector<int>& listenFds)
    {
        return SyscallResult<std::vector<int>>::success(listenFds);
    }

    static SyscallResult<bool> listening(bool isListening)
    {
        return SyscallResult<bool>::success(isListening);
    }

    int takeErrorValue() const
    {
        try
        {
            static_cast<void>(m_activation->takeListeningFd());
        }
        catch (const std::system_error& error)
        {
            return error.code().value();
        }
        return NO_ERROR_CAUGHT;
    }

    void SetUp() override
    {
        std::unique_ptr<MockSystemdDaemon> daemon = std::make_unique<MockSystemdDaemon>();
        m_daemon = daemon.get();
        m_activation = std::make_unique<adapter_socket::SystemdActivation>(std::move(daemon),
                                                                           m_descriptorSyscalls);
    }

    std::shared_ptr<MockDescriptorSyscalls> m_descriptorSyscalls =
        std::make_shared<MockDescriptorSyscalls>();
    MockSystemdDaemon* m_daemon = nullptr;
    std::unique_ptr<adapter_socket::SystemdActivation> m_activation;
};

TEST_F(SystemdActivationSyscallsTest, ReturnsOwnedNonBlockingFdForSingleListeningSocket)
{
    EXPECT_CALL(*m_daemon, takeListenFds()).WillOnce(Return(activated({FAKE_ACTIVATED_FD})));
    EXPECT_CALL(*m_daemon, isListeningUnixStream(FAKE_ACTIVATED_FD))
        .WillOnce(Return(listening(true)));
    EXPECT_CALL(*m_descriptorSyscalls, enableNonBlocking(FAKE_ACTIVATED_FD))
        .WillOnce(Return(std::error_code()));
    EXPECT_CALL(*m_descriptorSyscalls, closeDescriptor(FAKE_ACTIVATED_FD)).Times(1);

    const std::unique_ptr<adapter_socket::IFileDescriptor> activatedFd =
        m_activation->takeListeningFd();

    EXPECT_EQ(activatedFd->value(), FAKE_ACTIVATED_FD);
}

TEST_F(SystemdActivationSyscallsTest, ListenFdsFailureThrowsSystemError)
{
    EXPECT_CALL(*m_daemon, takeListenFds())
        .WillOnce(Return(
            SyscallResult<std::vector<int>>::failure(failure(std::errc::bad_file_descriptor))));
    EXPECT_CALL(*m_daemon, isListeningUnixStream(_)).Times(0);
    EXPECT_CALL(*m_descriptorSyscalls, enableNonBlocking(_)).Times(0);
    EXPECT_CALL(*m_descriptorSyscalls, closeDescriptor(_)).Times(0);

    EXPECT_EQ(takeErrorValue(), EBADF);
}

TEST_F(SystemdActivationSyscallsTest, MoreThanOneActivatedSocketIsRejected)
{
    EXPECT_CALL(*m_daemon, takeListenFds())
        .WillOnce(Return(activated({FAKE_ACTIVATED_FD, SECOND_FAKE_ACTIVATED_FD})));
    EXPECT_CALL(*m_daemon, isListeningUnixStream(_)).Times(0);
    EXPECT_CALL(*m_descriptorSyscalls, enableNonBlocking(_)).Times(0);
    EXPECT_CALL(*m_descriptorSyscalls, closeDescriptor(_)).Times(0);

    std::string message;
    try
    {
        static_cast<void>(m_activation->takeListeningFd());
    }
    catch (const adapter_socket::ActivationException& error)
    {
        message = error.what();
    }
    EXPECT_EQ(message, MULTIPLE_SOCKETS_MESSAGE);
}

TEST_F(SystemdActivationSyscallsTest, SocketTypeCheckFailureThrowsSystemErrorWithoutClosing)
{
    EXPECT_CALL(*m_daemon, takeListenFds()).WillOnce(Return(activated({FAKE_ACTIVATED_FD})));
    EXPECT_CALL(*m_daemon, isListeningUnixStream(FAKE_ACTIVATED_FD))
        .WillOnce(Return(SyscallResult<bool>::failure(failure(std::errc::bad_file_descriptor))));
    EXPECT_CALL(*m_descriptorSyscalls, enableNonBlocking(_)).Times(0);
    EXPECT_CALL(*m_descriptorSyscalls, closeDescriptor(_)).Times(0);

    EXPECT_EQ(takeErrorValue(), EBADF);
}

TEST_F(SystemdActivationSyscallsTest, NotListeningSocketIsRejectedWithoutClosing)
{
    EXPECT_CALL(*m_daemon, takeListenFds()).WillOnce(Return(activated({FAKE_ACTIVATED_FD})));
    EXPECT_CALL(*m_daemon, isListeningUnixStream(FAKE_ACTIVATED_FD))
        .WillOnce(Return(listening(false)));
    EXPECT_CALL(*m_descriptorSyscalls, enableNonBlocking(_)).Times(0);
    EXPECT_CALL(*m_descriptorSyscalls, closeDescriptor(_)).Times(0);

    EXPECT_THROW(static_cast<void>(m_activation->takeListeningFd()),
                 adapter_socket::ActivationException);
}

TEST_F(SystemdActivationSyscallsTest, NonBlockingFailureThrowsSystemErrorWithoutClosing)
{
    EXPECT_CALL(*m_daemon, takeListenFds()).WillOnce(Return(activated({FAKE_ACTIVATED_FD})));
    EXPECT_CALL(*m_daemon, isListeningUnixStream(FAKE_ACTIVATED_FD))
        .WillOnce(Return(listening(true)));
    EXPECT_CALL(*m_descriptorSyscalls, enableNonBlocking(FAKE_ACTIVATED_FD))
        .WillOnce(Return(failure(std::errc::bad_file_descriptor)));
    EXPECT_CALL(*m_descriptorSyscalls, closeDescriptor(_)).Times(0);

    EXPECT_EQ(takeErrorValue(), EBADF);
}
