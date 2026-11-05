#include "adapter_socket/socket_listener/unix_socket_listener.h"

#include "adapter_socket/file_descriptor/file_descriptor.h"
#include "adapter_socket/socket_connection/unix_socket_connection.h"
#include "adapter_socket/socket_syscalls/retryable_error.h"

#include <memory>
#include <system_error>
#include <utility>

namespace adapter_socket {

UnixSocketListener::UnixSocketListener(std::unique_ptr<IFileDescriptor> listeningFd,
                                       std::shared_ptr<ISocketSyscalls> socketSyscalls,
                                       std::shared_ptr<IDescriptorSyscalls> descriptorSyscalls)
    : m_listeningFd(std::move(listeningFd))
    , m_socketSyscalls(std::move(socketSyscalls))
    , m_descriptorSyscalls(std::move(descriptorSyscalls))
{
}

AcceptedConnectionDto UnixSocketListener::acceptOne(const IDeadline& deadline)
{
    const int listeningFd = m_listeningFd->value();

    while (true)
    {
        m_listeningFd->waitUntilReady(Readiness::Acceptable, deadline);

        const SyscallResult<int> accepted = m_socketSyscalls->acceptConnection(listeningFd);

        if (!accepted.hasFailed())
        {
            std::unique_ptr<IFileDescriptor> acceptedFd =
                std::make_unique<FileDescriptor>(accepted.value, m_descriptorSyscalls);
            const PeerCredentials peer = readPeerCredentials(*acceptedFd);

            AcceptedConnectionDto result;
            result.peer = peer;
            result.connection =
                std::make_unique<UnixSocketConnection>(std::move(acceptedFd), m_socketSyscalls);
            return result;
        }

        if (isRetryableAcceptError(accepted.error))
        {
            continue;
        }
        throw std::system_error(accepted.error, "accept4");
    }
}

PeerCredentials UnixSocketListener::readPeerCredentials(const IFileDescriptor& connectedFd) const
{
    const SyscallResult<PeerCredentials> peer =
        m_socketSyscalls->readPeerCredentials(connectedFd.value());
    if (peer.hasFailed())
    {
        throw std::system_error(peer.error, "getsockopt SO_PEERCRED");
    }
    return peer.value;
}

} // namespace adapter_socket
