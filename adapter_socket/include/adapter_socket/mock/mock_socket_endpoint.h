#pragma once

#include "adapter_socket/socket_endpoint/interface_socket_endpoint.h"

#include <gmock/gmock.h>

namespace adapter_socket {

class MockSocketEndpoint : public ISocketEndpoint
{
public:
    MOCK_METHOD(std::optional<std::unique_ptr<IFramedConnection>>, acceptAuthorized, (),
                (override));
};

} // namespace adapter_socket
