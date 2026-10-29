#include <adapter_socket/peer_connection/peer_credentials.h>

namespace adapter_socket {

PeerCredentials PeerCredentials::fromUcred(const ucred& credentials)
{
    PeerCredentials peer;
    peer.userId = credentials.uid;
    peer.groupId = credentials.gid;
    peer.processId = static_cast<uint32_t>(credentials.pid);
    return peer;
}

} // namespace adapter_socket
