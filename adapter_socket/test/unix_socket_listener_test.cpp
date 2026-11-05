#include <adapter_socket/deadline/interface_deadline.h>
#include <adapter_socket/deadline/interface_deadline_factory.h>
#include <adapter_socket/deadline/steady_deadline_factory.h>
#include <adapter_socket/file_descriptor/file_descriptor.h>
#include <adapter_socket/file_descriptor/interface_file_descriptor.h>
#include <adapter_socket/framed_connection/framed_connection.h>
#include <adapter_socket/mock/mock_deadline.h>
#include <adapter_socket/socket_connection/unix_socket_connection.h>
#include <adapter_socket/socket_listener/accepted_connection_dto.h>
#include <adapter_socket/socket_listener/unix_socket_listener.h>
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
using adapter_socket::IFileDescriptor;
using adapter_socket::ISocketConnection;
using adapter_socket::MockDeadline;
using adapter_socket::SteadyDeadlineFactory;
using adapter_socket::UnixSocketConnection;
using adapter_socket::UnixSocketListener;
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
};

TEST_F(UnixSocketListenerTest, AcceptsPendingClientAndReportsPeerCredentials)
{
    const int listeningFd = bindListeningSocket(m_socketPath);
    std::unique_ptr<IFileDescriptor> ownedListeningFd =
        std::make_unique<FileDescriptor>(listeningFd);
    UnixSocketListener listener(std::move(ownedListeningFd));
    const int clientFd = connectToListener(m_socketPath);
    std::unique_ptr<IFileDescriptor> ownedClientFd = std::make_unique<FileDescriptor>(clientFd);
    const UnixSocketConnection client(std::move(ownedClientFd));
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
        std::make_unique<FileDescriptor>(listeningFd);
    const std::shared_ptr<IDeadlineFactory> deadlineFactory =
        std::make_shared<SteadyDeadlineFactory>();
    UnixSocketListener listener(std::move(ownedListeningFd));
    const std::unique_ptr<IDeadline> acceptDeadline =
        deadlineFactory->startingNow(DEFAULT_TEST_TIMEOUT);
    const int clientFd = connectToListener(m_socketPath);
    std::unique_ptr<IFileDescriptor> ownedClientFd = std::make_unique<FileDescriptor>(clientFd);
    std::unique_ptr<ISocketConnection> clientConnection =
        std::make_unique<UnixSocketConnection>(std::move(ownedClientFd));
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
        std::make_unique<FileDescriptor>(listeningFd);
    UnixSocketListener listener(std::move(ownedListeningFd));

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
        std::make_unique<FileDescriptor>(listeningFd);
    {
        UnixSocketListener listener(std::move(ownedListeningFd));
    }

    EXPECT_TRUE(isFdClosed(listeningFd));
}
