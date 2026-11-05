#pragma once

#include "adapter_socket/deadline/interface_deadline.h"
#include "adapter_socket/file_descriptor/interface_file_descriptor.h"

#include <gmock/gmock.h>

namespace adapter_socket {

class MockFileDescriptor : public IFileDescriptor
{
public:
    MOCK_METHOD(int, value, (), (const, override));
    MOCK_METHOD(void, waitUntilReady, (Readiness readiness, const IDeadline& deadline),
                (const, override));
};

} // namespace adapter_socket
