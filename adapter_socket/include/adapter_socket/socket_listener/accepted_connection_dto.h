#ifndef ADAPTER_SOCKET_ACCEPTED_CONNECTION_DTO_H
#define ADAPTER_SOCKET_ACCEPTED_CONNECTION_DTO_H

#include <adapter_socket/peer_connection/peer_credentials.h>
#include <adapter_socket/socket_connection/interface_socket_connection.h>
#include <memory>

namespace adapter_socket {

struct AcceptedConnectionDto
{
    std::unique_ptr<ISocketConnection> connection;
    PeerCredentials peer;
};

} // namespace adapter_socket

#endif // ADAPTER_SOCKET_ACCEPTED_CONNECTION_DTO_H
