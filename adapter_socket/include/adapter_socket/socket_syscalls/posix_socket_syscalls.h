#ifndef ADAPTER_SOCKET_POSIX_SOCKET_SYSCALLS_H
#define ADAPTER_SOCKET_POSIX_SOCKET_SYSCALLS_H

#include "adapter_socket/socket_syscalls/interface_socket_syscalls.h"

namespace adapter_socket {

class PosixSocketSyscalls final : public ISocketSyscalls
{
public:
    PosixSocketSyscalls() = default;
    ~PosixSocketSyscalls() override = default;

    PosixSocketSyscalls(const PosixSocketSyscalls&) = delete;
    PosixSocketSyscalls& operator=(const PosixSocketSyscalls&) = delete;
    PosixSocketSyscalls(PosixSocketSyscalls&&) = delete;
    PosixSocketSyscalls& operator=(PosixSocketSyscalls&&) = delete;

    [[nodiscard]] SyscallResult<int> acceptConnection(int listeningFd) const override;
    [[nodiscard]] SyscallResult<std::size_t> receiveSome(
        int connectedFd, std::span<uint8_t> destination) const override;
    [[nodiscard]] SyscallResult<std::size_t> sendSome(
        int connectedFd, std::span<const uint8_t> source) const override;
    [[nodiscard]] SyscallResult<PeerCredentials> readPeerCredentials(
        int connectedFd) const override;
};

} // namespace adapter_socket

#endif /* ADAPTER_SOCKET_POSIX_SOCKET_SYSCALLS_H */
