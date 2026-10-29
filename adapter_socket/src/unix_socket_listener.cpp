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
            result.connection = std::make_unique<UnixSocketConnection>(std::move(acceptedFd));
            return result;
        }

        const int savedErrno = errno;
        const bool isRetryable = savedErrno == EAGAIN || savedErrno == EWOULDBLOCK ||
                                 savedErrno == EINTR || savedErrno == ECONNABORTED;
        if (isRetryable)
        {
            // LEARN: EAGAIN/EINTR/ECONNABORTED mean no connection was taken, so polling again
            // before the same deadline is safe.
            continue;
        }
        throw std::system_error(savedErrno, std::generic_category(), "accept4");
    }
}

PeerCredentials UnixSocketListener::readPeerCredentials(const IFileDescriptor& connectedFd)
{
    ucred credentials{};
    socklen_t length = sizeof(credentials);

    /* LEARN: getsockopt reads an option of the connected socket; the kernel writes the answer
       into the buffer we pass.
       SOL_SOCKET is the option level: the option belongs to the generic socket layer, not to
       one protocol (SOL_TCP, SOL_IP, ...).
       SO_PEERCRED is the option name (Linux): the pid, uid and gid the kernel recorded when the
       peer called connect(). The peer cannot forge them, and this is what the peer policy
       checks. The socket address is no substitute: an unbound AF_UNIX client has none.
       ucred is the exact type the kernel fills for SO_PEERCRED. It has three fields: pid, uid and gid.    
    */
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
