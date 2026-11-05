#ifndef ADAPTER_SOCKET_UNIX_SOCKET_LISTENER_FACTORY_H
#define ADAPTER_SOCKET_UNIX_SOCKET_LISTENER_FACTORY_H

#include <adapter_socket/file_descriptor/interface_file_descriptor.h>
#include <adapter_socket/socket_listener/interface_socket_listener_factory.h>
#include <adapter_socket/socket_listener/unix_socket_listener.h>
#include <memory>

namespace adapter_socket {

class UnixSocketListenerFactory final : public ISocketListenerFactory
{
public:
    UnixSocketListenerFactory() = default;
    ~UnixSocketListenerFactory() override = default;

    UnixSocketListenerFactory(const UnixSocketListenerFactory&) = delete;
    UnixSocketListenerFactory& operator=(const UnixSocketListenerFactory&) = delete;
    UnixSocketListenerFactory(UnixSocketListenerFactory&&) = delete;
    UnixSocketListenerFactory& operator=(UnixSocketListenerFactory&&) = delete;

    [[nodiscard]] std::unique_ptr<ISocketListener> create(
        std::unique_ptr<IFileDescriptor> listeningFd) const override;
};

} // namespace adapter_socket

#endif /* ADAPTER_SOCKET_UNIX_SOCKET_LISTENER_FACTORY_H */
