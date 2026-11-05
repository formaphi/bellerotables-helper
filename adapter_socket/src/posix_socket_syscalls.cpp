#include "adapter_socket/socket_syscalls/posix_socket_syscalls.h"

#include <cerrno>
#include <sys/socket.h>
#include <sys/types.h>
#include <system_error>

namespace adapter_socket {

constexpr int SYSCALL_ERROR = -1;
constexpr ssize_t SYSCALL_ERROR_SIZE = -1;

namespace {

std::error_code lastSystemError()
{
    const int savedErrno = errno;
    return {savedErrno, std::generic_category()};
}

} // namespace

SyscallResult<int> PosixSocketSyscalls::acceptConnection(int listeningFd) const
{
    const int accepted = ::accept4(listeningFd, nullptr, nullptr, SOCK_CLOEXEC);
    if (accepted == SYSCALL_ERROR)
    {
        return SyscallResult<int>::failure(lastSystemError());
    }
    return SyscallResult<int>::success(accepted);
}

SyscallResult<std::size_t> PosixSocketSyscalls::receiveSome(int connectedFd,
                                                            std::span<uint8_t> destination) const
{
    const ssize_t result =
        ::recv(connectedFd, destination.data(), destination.size(), MSG_DONTWAIT);
    if (result == SYSCALL_ERROR_SIZE)
    {
        return SyscallResult<std::size_t>::failure(lastSystemError());
    }
    return SyscallResult<std::size_t>::success(static_cast<std::size_t>(result));
}

SyscallResult<std::size_t> PosixSocketSyscalls::sendSome(int connectedFd,
                                                         std::span<const uint8_t> source) const
{
    /* LEARN: MSG_DONTWAIT makes this one send non-blocking: the accepted fd does not inherit
       O_NONBLOCK, and POLLOUT only promises some buffer space, so a blocking send of the rest
       would wait past the deadline. A partial send or EAGAIN goes back to poll instead.
       MSG_NOSIGNAL turns a write to a closed peer into EPIPE instead of SIGPIPE, whose default
       action kills the process without unwinding.
    */
    const int sendFlags = MSG_DONTWAIT | MSG_NOSIGNAL;
    const ssize_t result = ::send(connectedFd, source.data(), source.size(), sendFlags);
    if (result == SYSCALL_ERROR_SIZE)
    {
        return SyscallResult<std::size_t>::failure(lastSystemError());
    }
    return SyscallResult<std::size_t>::success(static_cast<std::size_t>(result));
}

SyscallResult<PeerCredentials> PosixSocketSyscalls::readPeerCredentials(int connectedFd) const
{
    ucred credentials{};
    socklen_t length = sizeof(credentials);

    /* LEARN: getsockopt reads an option of the connected socket; the kernel writes the answer
       into the buffer we pass.
       SOL_SOCKET is the option level: the option belongs to the generic socket layer, not to
       one protocol (SOL_TCP, SOL_IP, ...).
       SO_PEERCRED is the option name (Linux): the pid, uid and gid the kernel recorded when the
       peer called connect(). The peer cannot forge them, and this is what the peer policy
       checks. The socket address is no substitute: an unbound AF_UNIX client has none.
       ucred is the exact type the kernel fills for SO_PEERCRED. It has three fields: pid, uid and
       gid.
    */
    const int peerCredentialsResult =
        ::getsockopt(connectedFd, SOL_SOCKET, SO_PEERCRED, &credentials, &length);
    if (peerCredentialsResult == SYSCALL_ERROR)
    {
        return SyscallResult<PeerCredentials>::failure(lastSystemError());
    }

    return SyscallResult<PeerCredentials>::success(PeerCredentials::fromUcred(credentials));
}

} // namespace adapter_socket
