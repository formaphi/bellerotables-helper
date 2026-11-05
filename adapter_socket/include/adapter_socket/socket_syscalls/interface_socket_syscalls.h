#if !defined(INTERFACE_SOCKET_SYSCALLS_H)
#define INTERFACE_SOCKET_SYSCALLS_H

#include "adapter_socket/peer_connection/peer_credentials.h"
#include "adapter_socket/syscall_result/syscall_result.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace adapter_socket {

class ISocketSyscalls
{
public:
    virtual ~ISocketSyscalls() = default;

    [[nodiscard]] virtual SyscallResult<int> acceptConnection(int listeningFd) const = 0;
    [[nodiscard]] virtual SyscallResult<std::size_t> receiveSome(
        int connectedFd, std::span<uint8_t> destination) const = 0;
    [[nodiscard]] virtual SyscallResult<std::size_t> sendSome(
        int connectedFd, std::span<const uint8_t> source) const = 0;
    [[nodiscard]] virtual SyscallResult<PeerCredentials> readPeerCredentials(
        int connectedFd) const = 0;
};

} // namespace adapter_socket

#endif // INTERFACE_SOCKET_SYSCALLS_H
