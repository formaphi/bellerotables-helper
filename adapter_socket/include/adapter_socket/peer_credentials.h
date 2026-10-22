#ifndef ADAPTER_SOCKET_PEER_CREDENTIALS_H
#define ADAPTER_SOCKET_PEER_CREDENTIALS_H

#include <cstdint>

namespace adapter_socket {

struct PeerCredentials
{
    static constexpr uint32_t UNKNOWN_ID = 0;

    uint32_t userId    = UNKNOWN_ID;
    uint32_t groupId   = UNKNOWN_ID;
    uint32_t processId = UNKNOWN_ID;
};

} // namespace adapter_socket

#endif // ADAPTER_SOCKET_PEER_CREDENTIALS_H
