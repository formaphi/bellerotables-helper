#pragma once

#include <adapter_socket/interface_socket_connection.h>

#include <gmock/gmock.h>

namespace adapter_socket {

class MockSocketConnection : public ISocketConnection
{
public:
    MOCK_METHOD(std::vector<uint8_t>, readExactly, (std::size_t byteCount), (override));
    MOCK_METHOD(void, writeAll, (const std::vector<uint8_t>& bytes), (override));
    MOCK_METHOD(PeerCredentials, peerCredentials, (), (const, override));
};

} // namespace adapter_socket
