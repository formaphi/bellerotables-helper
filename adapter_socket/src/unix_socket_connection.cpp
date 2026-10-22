#include <adapter_socket/unix_socket_connection.h>

#include <adapter_socket/exceptions/socket_exception.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <limits>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>

namespace adapter_socket {

constexpr std::size_t NO_BYTES_TRANSFERRED = 0;
constexpr std::chrono::milliseconds NO_TIMEOUT = std::chrono::milliseconds(0);
constexpr nfds_t SINGLE_POLL_DESCRIPTOR = 1;
constexpr ssize_t PEER_CLOSED = 0;
constexpr int SYSCALL_ERROR = -1;
constexpr int POLL_TIMED_OUT = 0;
constexpr std::chrono::milliseconds::rep NO_TIME_REMAINING = 0;

static std::chrono::steady_clock::time_point deadlineAfter(std::chrono::milliseconds timeout)
{
    return std::chrono::steady_clock::now() + timeout;
}

static std::system_error deadlineExceeded(const char* operation)
{
    const std::string message = std::string(operation) + " deadline exceeded";
    return std::system_error(ETIMEDOUT, std::generic_category(), message);
}

UnixSocketConnection::UnixSocketConnection(int connectedFd, std::chrono::milliseconds timeout)
    : m_fd(connectedFd)
    , m_timeout(timeout)
    , m_peerCredentials()
{
    try
    {
        if (timeout <= NO_TIMEOUT)
        {
            throw std::invalid_argument("timeout must be greater than zero");
        }

        configureTimeouts(timeout);
        m_peerCredentials = readPeerCredentials();
    }
    catch (...)
    {
        closeFd();
        throw;
    }
}

UnixSocketConnection::~UnixSocketConnection()
{
    closeFd();
}

std::vector<uint8_t> UnixSocketConnection::readExactly(std::size_t byteCount)
{
    const std::chrono::steady_clock::time_point deadline = deadlineAfter(m_timeout);
    std::vector<uint8_t> bytes(byteCount);
    std::size_t received = NO_BYTES_TRANSFERRED;

    while (received < byteCount)
    {
        waitUntilReady(POLLIN, deadline, "read");

        const std::size_t remainingBytes = byteCount - received;
        uint8_t* const destination = bytes.data() + received;
        const ssize_t result = ::recv(m_fd, destination, remainingBytes, MSG_DONTWAIT);
        if (result == PEER_CLOSED)
        {
            throw ConnectionClosedException("peer closed the connection before all bytes arrived");
        }

        if (result == SYSCALL_ERROR)
        {
            const int savedErrno = errno;
            if (savedErrno == EAGAIN || savedErrno == EWOULDBLOCK)
            {
                continue;
            }
            if (savedErrno == EINTR)
            {
                // EINTR: a signal interrupted recv before any data moved, so retrying is safe and not an error.
                continue;
            }
            throw std::system_error(savedErrno, std::generic_category(), "recv");
        }

        received += static_cast<std::size_t>(result);
    }

    return bytes;
}

void UnixSocketConnection::writeAll(const std::vector<uint8_t>& bytes)
{
    const std::chrono::steady_clock::time_point deadline = deadlineAfter(m_timeout);
    const int sendFlags = MSG_DONTWAIT | MSG_NOSIGNAL;
    std::size_t sent = NO_BYTES_TRANSFERRED;

    while (sent < bytes.size())
    {
        waitUntilReady(POLLOUT, deadline, "write");

        const std::size_t remainingBytes = bytes.size() - sent;
        const uint8_t* const source = bytes.data() + sent;
        const ssize_t result = ::send(m_fd, source, remainingBytes, sendFlags);
        if (result == SYSCALL_ERROR)
        {
            const int savedErrno = errno;
            if (savedErrno == EAGAIN || savedErrno == EWOULDBLOCK)
            {
                continue;
            }
            if (savedErrno == EINTR)
            {
                // EINTR: a signal interrupted send before any data moved, so retrying is safe and not an error.
                continue;
            }
            throw std::system_error(savedErrno, std::generic_category(), "send");
        }

        sent += static_cast<std::size_t>(result);
    }
}

PeerCredentials UnixSocketConnection::peerCredentials() const
{
    return m_peerCredentials;
}

void UnixSocketConnection::configureTimeouts(std::chrono::milliseconds timeout)
{
    const std::chrono::seconds wholeSeconds = std::chrono::duration_cast<std::chrono::seconds>(timeout);
    const std::chrono::microseconds fraction = std::chrono::duration_cast<std::chrono::microseconds>(timeout - wholeSeconds);

    timeval socketTimeout{};
    socketTimeout.tv_sec = static_cast<time_t>(wholeSeconds.count());
    socketTimeout.tv_usec = static_cast<suseconds_t>(fraction.count());

    if (::setsockopt(m_fd, SOL_SOCKET, SO_RCVTIMEO, &socketTimeout, sizeof(socketTimeout)) == SYSCALL_ERROR)
    {
        const int savedErrno = errno;
        throw std::system_error(savedErrno, std::generic_category(), "setsockopt SO_RCVTIMEO");
    }

    if (::setsockopt(m_fd, SOL_SOCKET, SO_SNDTIMEO, &socketTimeout, sizeof(socketTimeout)) == SYSCALL_ERROR)
    {
        const int savedErrno = errno;
        throw std::system_error(savedErrno, std::generic_category(), "setsockopt SO_SNDTIMEO");
    }
}

void UnixSocketConnection::waitUntilReady(short pollEvents, const std::chrono::steady_clock::time_point& deadline, const char* operation) const
{
    pollfd descriptor{};
    descriptor.fd = m_fd;
    descriptor.events = pollEvents;

    while (true)
    {
        const std::chrono::steady_clock::duration untilDeadline = deadline - std::chrono::steady_clock::now();
        const std::chrono::milliseconds::rep remainingMilliseconds = std::chrono::ceil<std::chrono::milliseconds>(untilDeadline).count();
        if (remainingMilliseconds <= NO_TIME_REMAINING)
        {
            throw deadlineExceeded(operation);
        }

        const auto largestPollMilliseconds = static_cast<std::chrono::milliseconds::rep>(std::numeric_limits<int>::max());
        const std::chrono::milliseconds::rep boundedMilliseconds = std::min(remainingMilliseconds, largestPollMilliseconds);
        const int timeoutMilliseconds = static_cast<int>(boundedMilliseconds);

        const int pollResult = ::poll(&descriptor, SINGLE_POLL_DESCRIPTOR, timeoutMilliseconds);
        if (pollResult > POLL_TIMED_OUT)
        {
            return;
        }

        if (pollResult == POLL_TIMED_OUT)
        {
            // poll timed out on a millisecond boundary; the loop head decides whether the deadline really passed.
            continue;
        }

        const int savedErrno = errno;
        if (savedErrno == EINTR)
        {
            // EINTR: a signal interrupted poll before it reported readiness, so polling again is safe and not an error.
            continue;
        }
        throw std::system_error(savedErrno, std::generic_category(), "poll");
    }
}

PeerCredentials UnixSocketConnection::readPeerCredentials() const
{
    ucred credentials{};
    socklen_t length = sizeof(credentials);

    if (::getsockopt(m_fd, SOL_SOCKET, SO_PEERCRED, &credentials, &length) == SYSCALL_ERROR)
    {
        const int savedErrno = errno;
        throw std::system_error(savedErrno, std::generic_category(), "getsockopt SO_PEERCRED");
    }

    PeerCredentials peer;
    peer.userId = credentials.uid;
    peer.groupId = credentials.gid;
    peer.processId = static_cast<uint32_t>(credentials.pid);
    return peer;
}

void UnixSocketConnection::closeFd() noexcept
{
    if (m_fd != INVALID_FD)
    {
        ::close(m_fd);
        m_fd = INVALID_FD;
    }
}

} // namespace adapter_socket
