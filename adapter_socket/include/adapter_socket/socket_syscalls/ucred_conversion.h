#ifndef ADAPTER_SOCKET_UCRED_CONVERSION_H
#define ADAPTER_SOCKET_UCRED_CONVERSION_H

#include <adapter_socket/peer_connection/peer_credentials.h>

#include <sys/socket.h>
#include <cstdint>

namespace adapter_socket {

[[nodiscard]] inline PeerCredentials peerCredentialsFromUcred(const ucred& credentials)
{
    PeerCredentials peer;
    peer.userId = credentials.uid;
    peer.groupId = credentials.gid;
    peer.processId = static_cast<uint32_t>(credentials.pid);
    return peer;
}

} // namespace adapter_socket

#endif /* ADAPTER_SOCKET_UCRED_CONVERSION_H */
