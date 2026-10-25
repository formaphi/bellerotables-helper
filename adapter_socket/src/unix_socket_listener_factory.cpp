#include <adapter_socket/socket_listener/unix_socket_listener_factory.h>

#include <utility>

namespace adapter_socket {

std::shared_ptr<ISocketListener> UnixSocketListenerFactory::create(
    std::unique_ptr<IFileDescriptor> listeningFd) const
{
    return std::make_shared<UnixSocketListener>(std::move(listeningFd));
}

} // namespace adapter_socket
