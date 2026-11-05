#include "adapter_socket/socket_listener/unix_socket_listener_factory.h"

#include <utility>

namespace adapter_socket {

UnixSocketListenerFactory::UnixSocketListenerFactory(
    std::shared_ptr<ISocketSyscalls> socketSyscalls,
    std::shared_ptr<IDescriptorSyscalls> descriptorSyscalls)
    : m_socketSyscalls(std::move(socketSyscalls))
    , m_descriptorSyscalls(std::move(descriptorSyscalls))
{
}

std::unique_ptr<ISocketListener> UnixSocketListenerFactory::create(
    std::unique_ptr<IFileDescriptor> listeningFd) const
{
    return std::make_unique<UnixSocketListener>(std::move(listeningFd), m_socketSyscalls,
                                                m_descriptorSyscalls);
}

} // namespace adapter_socket
