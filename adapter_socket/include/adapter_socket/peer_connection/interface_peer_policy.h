#if !defined(INTERFACE_PEER_POLICY_H)
#define INTERFACE_PEER_POLICY_H

#include "adapter_socket/peer_connection/peer_credentials.h"

namespace adapter_socket {

class IPeerPolicy
{
public:
    virtual ~IPeerPolicy() = default;

    [[nodiscard]] virtual bool isAllowed(const PeerCredentials& peer) const = 0;
};

} // namespace adapter_socket

#endif // INTERFACE_PEER_POLICY_H
