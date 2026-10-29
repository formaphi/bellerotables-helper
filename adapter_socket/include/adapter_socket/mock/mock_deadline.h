#pragma once

#include <adapter_socket/deadline/interface_deadline.h>

#include <gmock/gmock.h>

namespace adapter_socket {

class MockDeadline : public IDeadline
{
public:
    MOCK_METHOD(bool, hasExpired, (), (const, override));
    MOCK_METHOD(int, pollTimeoutMilliseconds, (), (const, override));
};

} // namespace adapter_socket
