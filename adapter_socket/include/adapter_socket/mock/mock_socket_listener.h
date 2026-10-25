#pragma once

#include <adapter_socket/socket_listener/interface_socket_listener.h>

#include <gmock/gmock.h>

namespace adapter_socket {

class MockSocketListener : public ISocketListener
{
public:
    MOCK_METHOD(AcceptedConnectionDto, acceptOne, (const IDeadline& deadline), (override));
};

} // namespace adapter_socket
