#pragma once

#include <adapter_socket/framed_connection/interface_framed_connection.h>

#include <gmock/gmock.h>

namespace adapter_socket {

class MockFramedConnection : public IFramedConnection
{
public:
    MOCK_METHOD(std::vector<uint8_t>, readFrame, (), (override));
    MOCK_METHOD(void, writeFrame, (const std::vector<uint8_t>& payload), (override));
};

} // namespace adapter_socket
