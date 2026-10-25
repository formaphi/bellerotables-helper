#if !defined(INTERFACE_SOCKET_LISTENER_FACTORY_H)
#define INTERFACE_SOCKET_LISTENER_FACTORY_H

#include <adapter_socket/file_descriptor/interface_file_descriptor.h>
#include <adapter_socket/socket_listener/interface_socket_listener.h>

#include <memory>

namespace adapter_socket {

class ISocketListenerFactory
{
public:
    virtual ~ISocketListenerFactory() = default;

    [[nodiscard]] virtual std::shared_ptr<ISocketListener> create(
        std::unique_ptr<IFileDescriptor> listeningFd) const = 0;
};

} // namespace adapter_socket

#endif // INTERFACE_SOCKET_LISTENER_FACTORY_H
