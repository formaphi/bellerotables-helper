#include "adapter_socket/activation/systemd_activation.h"

#include "adapter_socket/exceptions/socket_exception.h"
#include "adapter_socket/file_descriptor/file_descriptor.h"

#include <cstddef>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace adapter_socket {

constexpr std::size_t EXPECTED_ACTIVATED_FDS = 1;

SystemdActivation::SystemdActivation(std::unique_ptr<ISystemdDaemon> systemdDaemon,
                                     std::shared_ptr<IDescriptorSyscalls> descriptorSyscalls)
    : m_systemdDaemon(std::move(systemdDaemon))
    , m_descriptorSyscalls(std::move(descriptorSyscalls))
{
}

std::unique_ptr<IFileDescriptor> SystemdActivation::takeListeningFd() const
{
    const SyscallResult<std::vector<int>> listenFds = m_systemdDaemon->takeListenFds();
    if (listenFds.hasFailed())
    {
        throw std::system_error(listenFds.error, "sd_listen_fds");
    }

    const std::size_t activatedCount = listenFds.value.size();
    if (activatedCount != EXPECTED_ACTIVATED_FDS)
    {
        throw ActivationException("expected exactly one activated socket, got " +
                                  std::to_string(activatedCount));
    }

    const int activatedFd = listenFds.value.front();
    const SyscallResult<bool> isListeningUnixStream =
        m_systemdDaemon->isListeningUnixStream(activatedFd);
    if (isListeningUnixStream.hasFailed())
    {
        throw std::system_error(isListeningUnixStream.error, "sd_is_socket_unix");
    }

    if (!isListeningUnixStream.value)
    {
        throw ActivationException("activated fd is not a listening AF_UNIX stream socket");
    }

    /*OPTIMIZE
        move this non-blocking switch to IFileDescriptor
        so that all activation won't responsible for this,
        and all file descriptors will be non-blocking by default
    */
    const std::error_code nonBlockingError = m_descriptorSyscalls->enableNonBlocking(activatedFd);
    if (nonBlockingError)
    {
        throw std::system_error(nonBlockingError, "fcntl O_NONBLOCK");
    }

    return std::make_unique<FileDescriptor>(activatedFd, m_descriptorSyscalls);
}

} // namespace adapter_socket
