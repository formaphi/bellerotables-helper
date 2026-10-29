#include <adapter_socket/socket_connection/unix_socket_connection.h>

#include <adapter_socket/exceptions/socket_exception.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <cerrno>
#include <system_error>
#include <utility>

namespace adapter_socket {

constexpr std::size_t NO_BYTES_TRANSFERRED = 0;
constexpr ssize_t PEER_CLOSED = 0;
constexpr int SYSCALL_ERROR = -1;

UnixSocketConnection::UnixSocketConnection(std::unique_ptr<IFileDescriptor> connectedFd)
    : m_connectedFd(std::move(connectedFd))
{
}

std::vector<uint8_t> UnixSocketConnection::readExactly(std::size_t byteCount,
                                                       const IDeadline& deadline)
{
    const int socketFd = m_connectedFd->value();
    std::vector<uint8_t> bytes(byteCount);
    std::size_t received = NO_BYTES_TRANSFERRED;

    while (received < byteCount)
    {
        m_connectedFd->waitUntilReady(Readiness::Readable, deadline);

        const std::size_t remainingBytes = byteCount - received;
        uint8_t* const destination = bytes.data() + received;
        const ssize_t result = ::recv(socketFd, destination, remainingBytes, MSG_DONTWAIT);
        if (result == PEER_CLOSED)
        {
            throw ConnectionClosedException("peer closed the connection before all bytes arrived");
        }

        if (result == SYSCALL_ERROR)
        {
            const int savedErrno = errno;
            /*OPTIMIZE:
             * We can create a proper error handling here
             * We could create a syscall handler for those implementations,
             * instead create Syscall interpretation for each situation
             */
            const bool wouldBlock = savedErrno == EAGAIN || savedErrno == EWOULDBLOCK;
            if (wouldBlock)
            {
                continue;
            }
            if (savedErrno == EINTR)
            {
                // EINTR: a signal interrupted recv before any data moved, so retrying is safe and
                // not an error.
                continue;
            }
            throw std::system_error(savedErrno, std::generic_category(), "recv");
        }

        received += static_cast<std::size_t>(result);
    }

    return bytes;
}

void UnixSocketConnection::writeAll(const std::vector<uint8_t>& bytes, const IDeadline& deadline)
{
    const int socketFd = m_connectedFd->value();
    /* LEARN: MSG_DONTWAIT makes this one send non-blocking: the accepted fd does not inherit
       O_NONBLOCK, and POLLOUT only promises some buffer space, so a blocking send of the rest
       would wait past the deadline. A partial send or EAGAIN goes back to poll instead.
       MSG_NOSIGNAL turns a write to a closed peer into EPIPE instead of SIGPIPE, whose default
       action kills the process without unwinding.
    */
    const int sendFlags = MSG_DONTWAIT | MSG_NOSIGNAL;
    std::size_t sent = NO_BYTES_TRANSFERRED;

    while (sent < bytes.size())
    {
        m_connectedFd->waitUntilReady(Readiness::Writable, deadline);

        const std::size_t remainingBytes = bytes.size() - sent;
        const uint8_t* const source = bytes.data() + sent;
        const ssize_t result = ::send(socketFd, source, remainingBytes, sendFlags);
        if (result == SYSCALL_ERROR)
        {
            const int savedErrno = errno;
            const bool wouldBlock = savedErrno == EAGAIN || savedErrno == EWOULDBLOCK;
            /*LEARN: it says "operation would block" but it is not an error, it just means
             that the socket buffer is full and we need to wait for it to be ready again.
            */

            if (wouldBlock)
            {
                continue;
            }
            if (savedErrno == EINTR)
            {
                // EINTR: a signal interrupted send before any data moved, so retrying is safe and
                // not an error.
                continue;
            }
            throw std::system_error(savedErrno, std::generic_category(), "send");
        }

        sent += static_cast<std::size_t>(result);
    }
}

} // namespace adapter_socket
