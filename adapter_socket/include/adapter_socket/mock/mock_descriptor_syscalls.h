#pragma once

#include "adapter_socket/descriptor_syscalls/interface_descriptor_syscalls.h"

#include <gmock/gmock.h>

namespace adapter_socket {

class MockDescriptorSyscalls : public IDescriptorSyscalls
{
public:
    MOCK_METHOD(void, closeDescriptor, (int fileDescriptor), (const, noexcept, override));
    MOCK_METHOD(SyscallResult<PollOutcome>, pollOnce,
                (int fileDescriptor, Readiness readiness, int timeoutMilliseconds),
                (const, override));
    MOCK_METHOD(std::error_code, enableNonBlocking, (int fileDescriptor), (const, override));
};

} // namespace adapter_socket
