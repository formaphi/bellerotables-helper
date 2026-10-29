#include <adapter_socket/activation/systemd_activation.h>

#include <adapter_socket/exceptions/socket_exception.h>
#include <adapter_socket/file_descriptor/file_descriptor.h>

#include <fcntl.h>
#include <sys/socket.h>
#include <systemd/sd-daemon.h>
#include <cerrno>
#include <cstddef>
#include <memory>
#include <string>
#include <system_error>

namespace adapter_socket {

constexpr int EXPECTED_ACTIVATED_FDS = 1;
constexpr int UNSET_ACTIVATION_ENVIRONMENT = 1;
constexpr int MUST_BE_LISTENING = 1;
constexpr int NOT_A_MATCH = 0;
constexpr int NO_ERROR_RESULT = 0;
constexpr int SYSCALL_ERROR = -1;
constexpr std::size_t NO_PATH_LENGTH = 0;

std::unique_ptr<IFileDescriptor> SystemdActivation::takeListeningFd() const
{
    const int activatedCount = ::sd_listen_fds(UNSET_ACTIVATION_ENVIRONMENT);
    if (activatedCount < NO_ERROR_RESULT)
    {
        throw std::system_error(-activatedCount, std::generic_category(), "sd_listen_fds");
    }

    if (activatedCount != EXPECTED_ACTIVATED_FDS)
    {
        throw ActivationException("expected exactly one activated socket, got " +
                                  std::to_string(activatedCount));
    }

    const int activatedFd = SD_LISTEN_FDS_START;
    const int isListeningUnixStream =
        ::sd_is_socket_unix(activatedFd, SOCK_STREAM, MUST_BE_LISTENING, nullptr, NO_PATH_LENGTH);
    if (isListeningUnixStream < NO_ERROR_RESULT)
    {
        throw std::system_error(-isListeningUnixStream, std::generic_category(),
                                "sd_is_socket_unix");
    }

    if (isListeningUnixStream == NOT_A_MATCH)
    {
        throw ActivationException("activated fd is not a listening AF_UNIX stream socket");
    }

    enableNonBlocking(activatedFd);
    return std::make_unique<FileDescriptor>(activatedFd);
}

void SystemdActivation::enableNonBlocking(int fileDescriptor)
{
    /*LEARN
        fcntl is a system call used to manipulate file descriptors. 
        In this context, it is used to read and modify the file status flags
        of the given file descriptor to enable non-blocking mode.    
    */
    /*OPTIMIZE
        move those two fcntl calls to IFileDescriptor 
        so that all activation won't responsible for this, 
        and all file descriptors will be non-blocking by default
    */

    const int flags = ::fcntl(fileDescriptor, F_GETFL);
    if (flags == SYSCALL_ERROR)
    {
        const int savedErrno = errno;
        throw std::system_error(savedErrno, std::generic_category(), "fcntl F_GETFL");
    }

    const int setResult = ::fcntl(fileDescriptor, F_SETFL, flags | O_NONBLOCK);
    if (setResult == SYSCALL_ERROR)
    {
        const int savedErrno = errno;
        throw std::system_error(savedErrno, std::generic_category(), "fcntl F_SETFL");
    }
}

} // namespace adapter_socket
