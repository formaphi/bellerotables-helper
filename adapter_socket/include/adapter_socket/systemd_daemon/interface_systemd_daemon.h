#if !defined(INTERFACE_SYSTEMD_DAEMON_H)
#define INTERFACE_SYSTEMD_DAEMON_H

#include "adapter_socket/syscall_result/syscall_result.h"

#include <vector>

namespace adapter_socket {

class ISystemdDaemon
{
public:
    virtual ~ISystemdDaemon() = default;

    [[nodiscard]] virtual SyscallResult<std::vector<int>> takeListenFds() const = 0;
    [[nodiscard]] virtual SyscallResult<bool> isListeningUnixStream(int fileDescriptor) const = 0;
};

} // namespace adapter_socket

#endif // INTERFACE_SYSTEMD_DAEMON_H
