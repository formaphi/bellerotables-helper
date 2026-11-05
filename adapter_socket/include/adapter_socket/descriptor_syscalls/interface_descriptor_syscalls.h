#if !defined(INTERFACE_DESCRIPTOR_SYSCALLS_H)
#define INTERFACE_DESCRIPTOR_SYSCALLS_H

#include "adapter_socket/descriptor_syscalls/poll_outcome.h"
#include "adapter_socket/file_descriptor/readiness.h"
#include "adapter_socket/syscall_result/syscall_result.h"

#include <system_error>

namespace adapter_socket {

class IDescriptorSyscalls
{
public:
    virtual ~IDescriptorSyscalls() = default;

    virtual void closeDescriptor(int fileDescriptor) const noexcept = 0;
    [[nodiscard]] virtual SyscallResult<PollOutcome> pollOnce(int fileDescriptor,
                                                              Readiness readiness,
                                                              int timeoutMilliseconds) const = 0;
    [[nodiscard]] virtual std::error_code enableNonBlocking(int fileDescriptor) const = 0;
};

} // namespace adapter_socket

#endif // INTERFACE_DESCRIPTOR_SYSCALLS_H
