#ifndef ADAPTER_SOCKET_SOCKET_ENDPOINT_H
#define ADAPTER_SOCKET_SOCKET_ENDPOINT_H

#include <adapter_socket/activation/interface_socket_activation.h>
#include <adapter_socket/deadline/interface_deadline_factory.h>
#include <adapter_socket/peer_connection/interface_peer_policy.h>
#include <adapter_socket/socket_endpoint/interface_socket_endpoint.h>
#include <adapter_socket/socket_endpoint/socket_timeouts_dto.h>
#include <adapter_socket/socket_listener/interface_socket_listener.h>
#include <adapter_socket/socket_listener/interface_socket_listener_factory.h>
#include <memory>
#include <optional>

namespace adapter_socket {

class SocketEndpoint final : public ISocketEndpoint
{
public:
    SocketEndpoint(const std::shared_ptr<ISocketActivation>& activation,
                   const std::shared_ptr<ISocketListenerFactory>& listenerFactory,
                   std::unique_ptr<IPeerPolicy> peerPolicy,
                   std::shared_ptr<IDeadlineFactory> deadlineFactory,
                   const SocketTimeoutsDto& timeouts = SocketTimeoutsDto());
    ~SocketEndpoint() override = default;

    SocketEndpoint(const SocketEndpoint&) = delete;
    SocketEndpoint& operator=(const SocketEndpoint&) = delete;
    SocketEndpoint(SocketEndpoint&&) = delete;
    SocketEndpoint& operator=(SocketEndpoint&&) = delete;

    std::optional<std::unique_ptr<IFramedConnection>> acceptAuthorized() override;

private:
    static std::unique_ptr<ISocketListener> createListener(
        const std::shared_ptr<ISocketActivation>& activation,
        const std::shared_ptr<ISocketListenerFactory>& listenerFactory);

    std::unique_ptr<ISocketListener> m_listener;
    std::unique_ptr<IPeerPolicy> m_peerPolicy;
    std::shared_ptr<IDeadlineFactory> m_deadlineFactory;
    SocketTimeoutsDto m_timeouts;
};

} // namespace adapter_socket

#endif /* ADAPTER_SOCKET_SOCKET_ENDPOINT_H */
