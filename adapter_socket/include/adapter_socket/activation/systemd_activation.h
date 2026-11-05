#ifndef ADAPTER_SOCKET_SYSTEMD_ACTIVATION_H
#define ADAPTER_SOCKET_SYSTEMD_ACTIVATION_H

#include "adapter_socket/activation/interface_socket_activation.h"
#include "adapter_socket/descriptor_syscalls/interface_descriptor_syscalls.h"
#include "adapter_socket/systemd_daemon/interface_systemd_daemon.h"

#include <memory>

namespace adapter_socket {

class SystemdActivation final : public ISocketActivation
{
public:
    SystemdActivation(std::unique_ptr<ISystemdDaemon> systemdDaemon,
                      std::shared_ptr<IDescriptorSyscalls> descriptorSyscalls);
    ~SystemdActivation() override = default;

    SystemdActivation(const SystemdActivation&) = delete;
    SystemdActivation& operator=(const SystemdActivation&) = delete;
    SystemdActivation(SystemdActivation&&) = delete;
    SystemdActivation& operator=(SystemdActivation&&) = delete;

    [[nodiscard]] std::unique_ptr<IFileDescriptor> takeListeningFd() const override;

private:
    std::unique_ptr<ISystemdDaemon> m_systemdDaemon;
    std::shared_ptr<IDescriptorSyscalls> m_descriptorSyscalls;
};

} // namespace adapter_socket

#endif /* ADAPTER_SOCKET_SYSTEMD_ACTIVATION_H */
