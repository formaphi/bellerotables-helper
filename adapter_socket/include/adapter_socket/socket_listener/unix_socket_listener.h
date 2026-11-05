#ifndef ADAPTER_SOCKET_UNIX_SOCKET_LISTENER_H
#define ADAPTER_SOCKET_UNIX_SOCKET_LISTENER_H

#include "adapter_socket/deadline/interface_deadline.h"
#include "adapter_socket/descriptor_syscalls/interface_descriptor_syscalls.h"
#include "adapter_socket/file_descriptor/interface_file_descriptor.h"
#include "adapter_socket/peer_connection/peer_credentials.h"
#include "adapter_socket/socket_listener/accepted_connection_dto.h"
#include "adapter_socket/socket_listener/interface_socket_listener.h"
#include "adapter_socket/socket_syscalls/interface_socket_syscalls.h"

#include <memory>

namespace adapter_socket {

class UnixSocketListener final : public ISocketListener
{
public:
    UnixSocketListener(std::unique_ptr<IFileDescriptor> listeningFd,
                       std::shared_ptr<ISocketSyscalls> socketSyscalls,
                       std::shared_ptr<IDescriptorSyscalls> descriptorSyscalls);
    ~UnixSocketListener() override = default;

    UnixSocketListener(const UnixSocketListener&) = delete;
    UnixSocketListener& operator=(const UnixSocketListener&) = delete;
    UnixSocketListener(UnixSocketListener&&) = delete;
    UnixSocketListener& operator=(UnixSocketListener&&) = delete;

    AcceptedConnectionDto acceptOne(const IDeadline& deadline) override;

private:
    [[nodiscard]] PeerCredentials readPeerCredentials(const IFileDescriptor& connectedFd) const;

    std::unique_ptr<IFileDescriptor> m_listeningFd;
    std::shared_ptr<ISocketSyscalls> m_socketSyscalls;
    std::shared_ptr<IDescriptorSyscalls> m_descriptorSyscalls;
};

} // namespace adapter_socket

#endif /* ADAPTER_SOCKET_UNIX_SOCKET_LISTENER_H */
