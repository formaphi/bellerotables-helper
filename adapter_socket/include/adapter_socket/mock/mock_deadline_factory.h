#pragma once

#include "adapter_socket/deadline/interface_deadline_factory.h"

#include <gmock/gmock.h>

namespace adapter_socket {

class MockDeadlineFactory : public IDeadlineFactory
{
public:
    MOCK_METHOD(std::unique_ptr<IDeadline>, startingNow, (std::chrono::milliseconds timeout),
                (const, override));
};

} // namespace adapter_socket
