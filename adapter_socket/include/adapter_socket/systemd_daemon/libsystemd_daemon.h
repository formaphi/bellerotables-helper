#ifndef ADAPTER_SOCKET_LIBSYSTEMD_DAEMON_H
#define ADAPTER_SOCKET_LIBSYSTEMD_DAEMON_H

#include "adapter_socket/systemd_daemon/interface_systemd_daemon.h"

namespace adapter_socket {

class LibsystemdDaemon final : public ISystemdDaemon
{
public:
    LibsystemdDaemon() = default;
    ~LibsystemdDaemon() override = default;

    LibsystemdDaemon(const LibsystemdDaemon&) = delete;
    LibsystemdDaemon& operator=(const LibsystemdDaemon&) = delete;
    LibsystemdDaemon(LibsystemdDaemon&&) = delete;
    LibsystemdDaemon& operator=(LibsystemdDaemon&&) = delete;

    [[nodiscard]] SyscallResult<std::vector<int>> takeListenFds() const override;
    [[nodiscard]] SyscallResult<bool> isListeningUnixStream(int fileDescriptor) const override;
};

} // namespace adapter_socket

#endif /* ADAPTER_SOCKET_LIBSYSTEMD_DAEMON_H */
