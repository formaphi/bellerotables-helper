#ifndef ADAPTER_SOCKET_POSIX_DESCRIPTOR_SYSCALLS_H
#define ADAPTER_SOCKET_POSIX_DESCRIPTOR_SYSCALLS_H

#include "adapter_socket/descriptor_syscalls/interface_descriptor_syscalls.h"

namespace adapter_socket {

class PosixDescriptorSyscalls final : public IDescriptorSyscalls
{
public:
    PosixDescriptorSyscalls() = default;
    ~PosixDescriptorSyscalls() override = default;

    PosixDescriptorSyscalls(const PosixDescriptorSyscalls&) = delete;
    PosixDescriptorSyscalls& operator=(const PosixDescriptorSyscalls&) = delete;
    PosixDescriptorSyscalls(PosixDescriptorSyscalls&&) = delete;
    PosixDescriptorSyscalls& operator=(PosixDescriptorSyscalls&&) = delete;

    void closeDescriptor(int fileDescriptor) const noexcept override;
    [[nodiscard]] SyscallResult<PollOutcome> pollOnce(int fileDescriptor, Readiness readiness,
                                                      int timeoutMilliseconds) const override;
    [[nodiscard]] std::error_code enableNonBlocking(int fileDescriptor) const override;

private:
    [[nodiscard]] static short pollEventsFor(Readiness readiness);
};

} // namespace adapter_socket

#endif /* ADAPTER_SOCKET_POSIX_DESCRIPTOR_SYSCALLS_H */
