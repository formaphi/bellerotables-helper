#include <adapter_socket/socket_listener/unix_socket_listener.h>

#include <adapter_socket/file_descriptor/file_descriptor.h>
#include <adapter_socket/socket_connection/unix_socket_connection.h>

#include <sys/socket.h>
#include <cerrno>
#include <memory>
#include <system_error>
#include <utility>

namespace adapter_socket {

constexpr int SYSCALL_ERROR = -1;

UnixSocketListener::UnixSocketListener(std::unique_ptr<IFileDescriptor> listeningFd)
    : m_listeningFd(std::move(listeningFd))
{
}

AcceptedConnectionDto UnixSocketListener::acceptOne(const IDeadline& deadline)
{
    const int listeningFd = m_listeningFd->value();

    while (true)
    {
        m_listeningFd->waitUntilReady(Readiness::Acceptable, deadline);

        const int accepted = ::accept4(listeningFd, nullptr, nullptr, SOCK_CLOEXEC);
        
        if (accepted != SYSCALL_ERROR)
        {
            std::unique_ptr<IFileDescriptor> acceptedFd =
                std::make_unique<FileDescriptor>(accepted);
            const PeerCredentials peer = readPeerCredentials(*acceptedFd);

            AcceptedConnectionDto result;
            result.peer = peer;
            result.connection = std::make_shared<UnixSocketConnection>(std::move(acceptedFd));
            return result;
        }

        const int savedErrno = errno;
        const bool isRetryable = savedErrno == EAGAIN || savedErrno == EWOULDBLOCK ||
                                 savedErrno == EINTR || savedErrno == ECONNABORTED;
        if (isRetryable)
        {
            // EAGAIN/EINTR/ECONNABORTED: no connection was taken, so polling again before the same
            // deadline is safe.
            continue;
        }
        throw std::system_error(savedErrno, std::generic_category(), "accept4");
    }
}

PeerCredentials UnixSocketListener::readPeerCredentials(const IFileDescriptor& connectedFd)
{
    ucred credentials{};
    socklen_t length = sizeof(credentials);

    const int peerCredentialsResult =
        ::getsockopt(connectedFd.value(), SOL_SOCKET, SO_PEERCRED, &credentials, &length);
    if (peerCredentialsResult == SYSCALL_ERROR)
    {
        const int savedErrno = errno;
        throw std::system_error(savedErrno, std::generic_category(), "getsockopt SO_PEERCRED");
    }

    return PeerCredentials::fromUcred(credentials);
}

} // namespace adapter_socket
