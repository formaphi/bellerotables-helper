#pragma once

#include <adapter_socket/socket_connection/interface_socket_connection.h>

#include <gmock/gmock.h>

namespace adapter_socket {

class MockSocketConnection : public ISocketConnection
{
public:
    MOCK_METHOD(std::vector<uint8_t>, readExactly,
                (std::size_t byteCount, const IDeadline& deadline), (override));
    MOCK_METHOD(void, writeAll, (const std::vector<uint8_t>& bytes, const IDeadline& deadline),
                (override));
};

} // namespace adapter_socket
