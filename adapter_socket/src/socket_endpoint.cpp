#include <adapter_socket/socket_endpoint/socket_endpoint.h>

#include <adapter_socket/file_descriptor/interface_file_descriptor.h>
#include <adapter_socket/framed_connection/framed_connection.h>

#include <utility>

namespace adapter_socket {

SocketEndpoint::SocketEndpoint(const std::shared_ptr<ISocketActivation>& activation,
                               const std::shared_ptr<ISocketListenerFactory>& listenerFactory,
                               std::unique_ptr<IPeerPolicy> peerPolicy,
                               std::shared_ptr<IDeadlineFactory> deadlineFactory,
                               const SocketTimeoutsDto& timeouts)
    : m_listener(createListener(activation, listenerFactory))
    , m_peerPolicy(std::move(peerPolicy))
    , m_deadlineFactory(std::move(deadlineFactory))
    , m_timeouts(timeouts)
{
}

std::unique_ptr<ISocketListener> SocketEndpoint::createListener(
    const std::shared_ptr<ISocketActivation>& activation,
    const std::shared_ptr<ISocketListenerFactory>& listenerFactory)
{
    std::unique_ptr<IFileDescriptor> listeningFd = activation->takeListeningFd();
    return listenerFactory->create(std::move(listeningFd));
}

std::optional<std::unique_ptr<IFramedConnection>> SocketEndpoint::acceptAuthorized()
{
    const std::unique_ptr<IDeadline> acceptDeadline =
        m_deadlineFactory->startingNow(m_timeouts.acceptTimeout);
    AcceptedConnectionDto accepted = m_listener->acceptOne(*acceptDeadline);
    const bool isAuthorized = m_peerPolicy->isAllowed(accepted.peer);
    if (!isAuthorized)
    {
        return std::nullopt;
    }
    return std::make_unique<FramedConnection>(std::move(accepted.connection), m_deadlineFactory,
                                              m_timeouts.connectionTimeout);
}

} // namespace adapter_socket
