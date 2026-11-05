#pragma once

#include "adapter_socket/socket_listener/interface_socket_listener_factory.h"

#include <gmock/gmock.h>

namespace adapter_socket {

class MockSocketListenerFactory : public ISocketListenerFactory
{
public:
    MOCK_METHOD(std::unique_ptr<ISocketListener>, create,
                (std::unique_ptr<IFileDescriptor> listeningFd), (const, override));
};

} // namespace adapter_socket
