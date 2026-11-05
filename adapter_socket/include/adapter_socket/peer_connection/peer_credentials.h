#ifndef ADAPTER_SOCKET_PEER_CREDENTIALS_H
#define ADAPTER_SOCKET_PEER_CREDENTIALS_H

#include <cstdint>

/* LEARN: ucred is the C struct the kernel fills for SO_PEERCRED (<sys/socket.h>). A forward
   declaration is enough to name it here, so including this header does not pull the C socket API
   into the domain code; only peer_credentials.cpp includes <sys/socket.h>.
*/
struct ucred;

namespace adapter_socket {

struct PeerCredentials
{
    static constexpr uint32_t UNKNOWN_ID = 0;

    uint32_t userId = UNKNOWN_ID;
    uint32_t groupId = UNKNOWN_ID;
    uint32_t processId = UNKNOWN_ID;

    [[nodiscard]] static PeerCredentials fromUcred(const ucred& credentials);
};

} // namespace adapter_socket

#endif // ADAPTER_SOCKET_PEER_CREDENTIALS_H
