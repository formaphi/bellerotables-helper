#pragma once

#include "adapter_socket/peer_connection/interface_peer_policy.h"

#include <gmock/gmock.h>

namespace adapter_socket {

class MockPeerPolicy : public IPeerPolicy
{
public:
    MOCK_METHOD(bool, isAllowed, (const PeerCredentials& peer), (const, override));
};

} // namespace adapter_socket
