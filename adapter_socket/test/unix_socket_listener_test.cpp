#include "adapter_socket/socket_listener/unix_socket_listener.h"

#include "adapter_socket/deadline/interface_deadline.h"
#include "adapter_socket/deadline/interface_deadline_factory.h"
#include "adapter_socket/deadline/steady_deadline_factory.h"
#include "adapter_socket/descriptor_syscalls/interface_descriptor_syscalls.h"
#include "adapter_socket/descriptor_syscalls/posix_descriptor_syscalls.h"
#include "adapter_socket/file_descriptor/file_descriptor.h"
#include "adapter_socket/file_descriptor/interface_file_descriptor.h"
#include "adapter_socket/framed_connection/framed_connection.h"
#include "adapter_socket/mock/mock_deadline.h"
#include "adapter_socket/mock/mock_descriptor_syscalls.h"
#include "adapter_socket/mock/mock_file_descriptor.h"
#include "adapter_socket/mock/mock_socket_syscalls.h"
#include "adapter_socket/socket_connection/unix_socket_connection.h"
#include "adapter_socket/socket_listener/accepted_connection_dto.h"
#include "adapter_socket/socket_syscalls/interface_socket_syscalls.h"
#include "adapter_socket/socket_syscalls/posix_socket_syscalls.h"
#include "parameterized_case.h"

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
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
#include <unistd.h>
#include <utility>
#include <vector>

using adapter_socket::AcceptedConnectionDto;
using adapter_socket::FileDescriptor;
using adapter_socket::FramedConnection;
using adapter_socket::IDeadline;
using adapter_socket::IDeadlineFactory;
using adapter_socket::IDescriptorSyscalls;
using adapter_socket::IFileDescriptor;
using adapter_socket::ISocketConnection;
using adapter_socket::ISocketSyscalls;
using adapter_socket::MockDeadline;
using adapter_socket::MockDescriptorSyscalls;
using adapter_socket::MockFileDescriptor;
using adapter_socket::MockSocketSyscalls;
using adapter_socket::PeerCredentials;
using adapter_socket::PosixDescriptorSyscalls;
using adapter_socket::PosixSocketSyscalls;
using adapter_socket::Readiness;
using adapter_socket::SteadyDeadlineFactory;
using adapter_socket::SyscallResult;
using adapter_socket::UnixSocketConnection;
using adapter_socket::UnixSocketListener;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;

constexpr std::chrono::milliseconds DEFAULT_TEST_TIMEOUT(5000);
constexpr std::chrono::milliseconds FRAME_TIMEOUT(5000);
constexpr int LISTEN_BACKLOG = 4;
constexpr int DEFAULT_PROTOCOL = 0;
constexpr int SYSCALL_ERROR = -1;
constexpr std::string_view TEMP_DIRECTORY_TEMPLATE = "/tmp/adapter_socket_listener_XXXXXX";
constexpr std::string_view SOCKET_FILE_NAME = "helper.sock";
constexpr std::size_t ROUND_TRIP_PAYLOAD_BYTES = 3;
constexpr uint8_t ROUND_TRIP_PAYLOAD_BYTE = 0x5A;
constexpr int FAKE_LISTENING_FD = 4240;
constexpr int FAKE_ACCEPTED_FD = 4241;
constexpr int NO_ERROR_CAUGHT = 0;
constexpr uint32_t AGENT_USER_ID = 900;
constexpr uint32_t AGENT_GROUP_ID = 901;
constexpr uint32_t AGENT_PROCESS_ID = 4242;
constexpr PeerCredentials AGENT_PEER{AGENT_USER_ID, AGENT_GROUP_ID, AGENT_PROCESS_ID};

class UnixSocketListenerTest : public ::testing::Test
{
protected:
    [[noreturn]] static void throwSystemError(const char* operation)
    {
        const int savedErrno = errno;
        throw std::system_error(savedErrno, std::generic_category(), operation);
    }

    static std::unique_ptr<IDeadline> expiredDeadline()
    {
        std::unique_ptr<MockDeadline> deadline = std::make_unique<MockDeadline>();
        EXPECT_CALL(*deadline, hasExpired()).WillRepeatedly(Return(true));
        return deadline;
    }

    static bool isFdClosed(int descriptor)
    {
        const int descriptorFlags = ::fcntl(descriptor, F_GETFD);
        const int savedErrno = errno;
        return descriptorFlags == SYSCALL_ERROR && savedErrno == EBADF;
    }

    static int createStreamSocket()
    {
        const int socketFd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, DEFAULT_PROTOCOL);
        if (socketFd == SYSCALL_ERROR)
        {
            throwSystemError("socket");
        }
        return socketFd;
    }

    static sockaddr_un makeSocketAddress(const std::string& path)
    {
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        std::memcpy(address.sun_path, path.c_str(), path.size());
        return address;
    }

    static int bindListeningSocket(const std::string& path)
    {
        const int listeningFd = createStreamSocket();
        const sockaddr_un address = makeSocketAddress(path);
        const int bindResult =
            ::bind(listeningFd, reinterpret_cast<const sockaddr*>(&address), sizeof(address));
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

    static int connectToListener(const std::string& path)
    {
        const int clientFd = createStreamSocket();
        const sockaddr_un address = makeSocketAddress(path);
        const int connectResult =
            ::connect(clientFd, reinterpret_cast<const sockaddr*>(&address), sizeof(address));
        if (connectResult == SYSCALL_ERROR)
        {
            const int savedErrno = errno;
            ::close(clientFd);
            throw std::system_error(savedErrno, std::generic_category(), "connect");
        }
        return clientFd;
    }

    void SetUp() override
    {
        std::string directoryTemplate(TEMP_DIRECTORY_TEMPLATE);
        const char* const createdDirectory = ::mkdtemp(directoryTemplate.data());
        if (createdDirectory == nullptr)
        {
            throwSystemError("mkdtemp");
        }

        m_directory = directoryTemplate;
        m_socketPath = m_directory + "/" + std::string(SOCKET_FILE_NAME);
    }

    void TearDown() override
    {
        ::unlink(m_socketPath.c_str());
        ::rmdir(m_directory.c_str());
    }

    std::string m_directory;
    std::string m_socketPath;
    std::shared_ptr<IDescriptorSyscalls> m_descriptorSyscalls =
        std::make_shared<PosixDescriptorSyscalls>();
    std::shared_ptr<ISocketSyscalls> m_socketSyscalls = std::make_shared<PosixSocketSyscalls>();
};

TEST_F(UnixSocketListenerTest, AcceptsPendingClientAndReportsPeerCredentials)
{
    const int listeningFd = bindListeningSocket(m_socketPath);
    std::unique_ptr<IFileDescriptor> ownedListeningFd =
        std::make_unique<FileDescriptor>(listeningFd, m_descriptorSyscalls);
    UnixSocketListener listener(std::move(ownedListeningFd), m_socketSyscalls,
                                m_descriptorSyscalls);
    const int clientFd = connectToListener(m_socketPath);
    std::unique_ptr<IFileDescriptor> ownedClientFd =
        std::make_unique<FileDescriptor>(clientFd, m_descriptorSyscalls);
    const UnixSocketConnection client(std::move(ownedClientFd), m_socketSyscalls);
    const SteadyDeadlineFactory deadlineFactory;
    const std::unique_ptr<IDeadline> deadline = deadlineFactory.startingNow(DEFAULT_TEST_TIMEOUT);

    const AcceptedConnectionDto accepted = listener.acceptOne(*deadline);

    ASSERT_NE(accepted.connection.get(), nullptr);
    const auto currentUserId = static_cast<uint32_t>(::getuid());
    EXPECT_EQ(accepted.peer.userId, currentUserId);
}

TEST_F(UnixSocketListenerTest, AcceptedConnectionCarriesFramedPayloadFromClient)
{
    const int listeningFd = bindListeningSocket(m_socketPath);
    std::unique_ptr<IFileDescriptor> ownedListeningFd =
        std::make_unique<FileDescriptor>(listeningFd, m_descriptorSyscalls);
    const std::shared_ptr<IDeadlineFactory> deadlineFactory =
        std::make_shared<SteadyDeadlineFactory>();
    UnixSocketListener listener(std::move(ownedListeningFd), m_socketSyscalls,
                                m_descriptorSyscalls);
    const std::unique_ptr<IDeadline> acceptDeadline =
        deadlineFactory->startingNow(DEFAULT_TEST_TIMEOUT);
    const int clientFd = connectToListener(m_socketPath);
    std::unique_ptr<IFileDescriptor> ownedClientFd =
        std::make_unique<FileDescriptor>(clientFd, m_descriptorSyscalls);
    std::unique_ptr<ISocketConnection> clientConnection =
        std::make_unique<UnixSocketConnection>(std::move(ownedClientFd), m_socketSyscalls);
    FramedConnection clientFramed(std::move(clientConnection), deadlineFactory, FRAME_TIMEOUT);
    const std::vector<uint8_t> payload(ROUND_TRIP_PAYLOAD_BYTES, ROUND_TRIP_PAYLOAD_BYTE);
    clientFramed.writeFrame(payload);

    AcceptedConnectionDto accepted = listener.acceptOne(*acceptDeadline);
    FramedConnection serverFramed(std::move(accepted.connection), deadlineFactory, FRAME_TIMEOUT);
    const std::vector<uint8_t> received = serverFramed.readFrame();
    EXPECT_EQ(received, payload);
}

TEST_F(UnixSocketListenerTest, AcceptWithoutPendingClientTimesOut)
{
    const int listeningFd = bindListeningSocket(m_socketPath);
    std::unique_ptr<IFileDescriptor> ownedListeningFd =
        std::make_unique<FileDescriptor>(listeningFd, m_descriptorSyscalls);
    UnixSocketListener listener(std::move(ownedListeningFd), m_socketSyscalls,
                                m_descriptorSyscalls);

    bool timedOut = false;
    try
    {
        static_cast<void>(listener.acceptOne(*expiredDeadline()));
    }
    catch (const std::system_error& error)
    {
        timedOut = error.code().value() == ETIMEDOUT;
    }

    EXPECT_TRUE(timedOut);
}

TEST_F(UnixSocketListenerTest, DestructorClosesListeningFd)
{
    const int listeningFd = bindListeningSocket(m_socketPath);
    std::unique_ptr<IFileDescriptor> ownedListeningFd =
        std::make_unique<FileDescriptor>(listeningFd, m_descriptorSyscalls);
    {
        UnixSocketListener listener(std::move(ownedListeningFd), m_socketSyscalls,
                                    m_descriptorSyscalls);
    }

    EXPECT_TRUE(isFdClosed(listeningFd));
}

class UnixSocketListenerSyscallsTest : public ::testing::Test
{
protected:
    /* LEARN: acceptOne only accepts and reads the peer's credentials. No payload byte moves before
       the endpoint has authorized the peer.
    */
    void SetUp() override
    {
        EXPECT_CALL(*m_socketSyscalls, receiveSome(_, _)).Times(0);
        EXPECT_CALL(*m_socketSyscalls, sendSome(_, _)).Times(0);
    }

    static SyscallResult<int> acceptFailure(std::errc error)
    {
        return SyscallResult<int>::failure(std::make_error_code(error));
    }

    std::unique_ptr<UnixSocketListener> buildListener()
    {
        std::unique_ptr<MockFileDescriptor> listeningFd = std::make_unique<MockFileDescriptor>();
        EXPECT_CALL(*listeningFd, value()).WillRepeatedly(Return(FAKE_LISTENING_FD));
        EXPECT_CALL(*listeningFd, waitUntilReady(Readiness::Acceptable, _))
            .WillRepeatedly(Return());
        return std::make_unique<UnixSocketListener>(std::move(listeningFd), m_socketSyscalls,
                                                    m_descriptorSyscalls);
    }

    int acceptErrorValue(UnixSocketListener& listener)
    {
        try
        {
            static_cast<void>(listener.acceptOne(m_deadline));
        }
        catch (const std::system_error& error)
        {
            return error.code().value();
        }
        return NO_ERROR_CAUGHT;
    }

    std::shared_ptr<MockSocketSyscalls> m_socketSyscalls = std::make_shared<MockSocketSyscalls>();
    std::shared_ptr<MockDescriptorSyscalls> m_descriptorSyscalls =
        std::make_shared<MockDescriptorSyscalls>();
    MockDeadline m_deadline;
};

TEST_F(UnixSocketListenerSyscallsTest, PeerCredentialsFailureClosesAcceptedFdAndThrows)
{
    EXPECT_CALL(*m_socketSyscalls, acceptConnection(FAKE_LISTENING_FD))
        .WillOnce(Return(SyscallResult<int>::success(FAKE_ACCEPTED_FD)));
    EXPECT_CALL(*m_socketSyscalls, readPeerCredentials(FAKE_ACCEPTED_FD))
        .WillOnce(Return(SyscallResult<PeerCredentials>::failure(
            std::make_error_code(std::errc::not_connected))));
    EXPECT_CALL(*m_descriptorSyscalls, closeDescriptor(FAKE_ACCEPTED_FD)).Times(1);
    const std::unique_ptr<UnixSocketListener> listener = buildListener();

    EXPECT_EQ(acceptErrorValue(*listener), ENOTCONN);
}

const std::vector<ErrnoCase> RETRYABLE_ACCEPT_ERRORS{
    {std::errc::resource_unavailable_try_again, "EAGAIN"},
    {std::errc::operation_would_block, "EWOULDBLOCK"},
    {std::errc::interrupted, "EINTR"},
    {std::errc::connection_aborted, "ECONNABORTED"},
};

const std::vector<ErrnoCase> FATAL_ACCEPT_ERRORS{
    {std::errc::too_many_files_open, "EMFILE"},
    {std::errc::too_many_files_open_in_system, "ENFILE"},
    {std::errc::no_buffer_space, "ENOBUFS"},
    {std::errc::bad_file_descriptor, "EBADF"},
    {std::errc::invalid_argument, "EINVAL"},
};

class UnixSocketListenerRetryableErrorTest : public UnixSocketListenerSyscallsTest,
                                             public ::testing::WithParamInterface<ErrnoCase>
{
};

class UnixSocketListenerFatalErrorTest : public UnixSocketListenerSyscallsTest,
                                         public ::testing::WithParamInterface<ErrnoCase>
{
};

TEST_P(UnixSocketListenerRetryableErrorTest, RetriesThenReportsPeer)
{
    {
        const InSequence inOrder;
        EXPECT_CALL(*m_socketSyscalls, acceptConnection(FAKE_LISTENING_FD))
            .WillOnce(Return(acceptFailure(GetParam().error)));
        EXPECT_CALL(*m_socketSyscalls, acceptConnection(FAKE_LISTENING_FD))
            .WillOnce(Return(SyscallResult<int>::success(FAKE_ACCEPTED_FD)));
    }
    EXPECT_CALL(*m_socketSyscalls, readPeerCredentials(FAKE_ACCEPTED_FD))
        .WillOnce(Return(SyscallResult<PeerCredentials>::success(AGENT_PEER)));
    EXPECT_CALL(*m_descriptorSyscalls, closeDescriptor(FAKE_ACCEPTED_FD)).Times(1);
    const std::unique_ptr<UnixSocketListener> listener = buildListener();

    const AcceptedConnectionDto accepted = listener->acceptOne(m_deadline);

    ASSERT_NE(accepted.connection.get(), nullptr);
    EXPECT_EQ(accepted.peer.userId, AGENT_USER_ID);
    EXPECT_EQ(accepted.peer.groupId, AGENT_GROUP_ID);
    EXPECT_EQ(accepted.peer.processId, AGENT_PROCESS_ID);
}

TEST_P(UnixSocketListenerFatalErrorTest, ThrowsTheAcceptErrnoWithoutTakingAConnection)
{
    EXPECT_CALL(*m_socketSyscalls, acceptConnection(FAKE_LISTENING_FD))
        .WillOnce(Return(acceptFailure(GetParam().error)));
    EXPECT_CALL(*m_socketSyscalls, readPeerCredentials(_)).Times(0);
    EXPECT_CALL(*m_descriptorSyscalls, closeDescriptor(_)).Times(0);
    const std::unique_ptr<UnixSocketListener> listener = buildListener();

    EXPECT_EQ(acceptErrorValue(*listener), static_cast<int>(GetParam().error));
}

INSTANTIATE_TEST_SUITE_P(RetryableErrors, UnixSocketListenerRetryableErrorTest,
                         ::testing::ValuesIn(RETRYABLE_ACCEPT_ERRORS), caseName<ErrnoCase>);

INSTANTIATE_TEST_SUITE_P(FatalErrors, UnixSocketListenerFatalErrorTest,
                         ::testing::ValuesIn(FATAL_ACCEPT_ERRORS), caseName<ErrnoCase>);
