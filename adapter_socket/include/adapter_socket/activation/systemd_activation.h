#ifndef ADAPTER_SOCKET_SYSTEMD_ACTIVATION_H
#define ADAPTER_SOCKET_SYSTEMD_ACTIVATION_H

#include <adapter_socket/activation/interface_socket_activation.h>

#include <memory>

namespace adapter_socket {

class SystemdActivation final : public ISocketActivation
{
public:
    SystemdActivation() = default;
    ~SystemdActivation() override = default;

    SystemdActivation(const SystemdActivation&) = delete;
    SystemdActivation& operator=(const SystemdActivation&) = delete;
    SystemdActivation(SystemdActivation&&) = delete;
    SystemdActivation& operator=(SystemdActivation&&) = delete;

    [[nodiscard]] std::unique_ptr<IFileDescriptor> takeListeningFd() const override;

private:
    static void enableNonBlocking(int fileDescriptor);
};

} // namespace adapter_socket

#endif /* ADAPTER_SOCKET_SYSTEMD_ACTIVATION_H */
