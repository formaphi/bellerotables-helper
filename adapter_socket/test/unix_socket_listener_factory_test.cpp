#include <adapter_socket/socket_listener/unix_socket_listener_factory.h>

#include <adapter_socket/file_descriptor/file_descriptor.h>
#include <adapter_socket/file_descriptor/interface_file_descriptor.h>
#include <adapter_socket/socket_listener/interface_socket_listener.h>

#include <gtest/gtest.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string_view>
#include <system_error>
#include <utility>

using adapter_socket::FileDescriptor;
using adapter_socket::IFileDescriptor;

constexpr int DEFAULT_PROTOCOL = 0;
constexpr int SYSCALL_ERROR = -1;
constexpr int LISTEN_BACKLOG = 4;
constexpr std::size_t ABSTRACT_NAME_INDEX = 1;
constexpr std::string_view ABSTRACT_SOCKET_NAME = "adapter_socket_factory_test";

class UnixSocketListenerFactoryTest : public ::testing::Test
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
};

TEST_F(UnixSocketListenerFactoryTest, CreatesListener)
{
    const adapter_socket::UnixSocketListenerFactory factory;
    const int listeningFd = createAbstractListeningSocket();

    std::unique_ptr<IFileDescriptor> ownedListeningFd =
        std::make_unique<FileDescriptor>(listeningFd);
    const std::shared_ptr<adapter_socket::ISocketListener> listener =
        factory.create(std::move(ownedListeningFd));

    EXPECT_NE(listener.get(), nullptr);
}
