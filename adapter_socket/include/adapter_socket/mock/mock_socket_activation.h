#pragma once

#include <adapter_socket/activation/interface_socket_activation.h>

#include <gmock/gmock.h>

namespace adapter_socket {

class MockSocketActivation : public ISocketActivation
{
public:
    MOCK_METHOD(std::unique_ptr<IFileDescriptor>, takeListeningFd, (), (const, override));
};

} // namespace adapter_socket
