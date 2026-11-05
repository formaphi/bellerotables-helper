#include "adapter_socket/socket_connection/unix_socket_connection.h"

#include "adapter_socket/exceptions/socket_exception.h"
#include "adapter_socket/socket_syscalls/retryable_error.h"

#include <span>
#include <system_error>
#include <utility>

namespace adapter_socket {

constexpr std::size_t NO_BYTES_TRANSFERRED = 0;
constexpr std::size_t PEER_CLOSED = 0;

UnixSocketConnection::UnixSocketConnection(std::unique_ptr<IFileDescriptor> connectedFd,
                                           std::shared_ptr<ISocketSyscalls> syscalls)
    : m_connectedFd(std::move(connectedFd))
    , m_syscalls(std::move(syscalls))
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

        const std::span<uint8_t> destination = std::span(bytes).subspan(received);
        const SyscallResult<std::size_t> result = m_syscalls->receiveSome(socketFd, destination);
        if (result.hasFailed())
        {
            if (isRetryableTransferError(result.error))
            {
                continue;
            }
            throw std::system_error(result.error, "recv");
        }

        if (result.value == PEER_CLOSED)
        {
            throw ConnectionClosedException("peer closed the connection before all bytes arrived");
        }

        received += result.value;
    }

    return bytes;
}

void UnixSocketConnection::writeAll(const std::vector<uint8_t>& bytes, const IDeadline& deadline)
{
    const int socketFd = m_connectedFd->value();
    std::size_t sent = NO_BYTES_TRANSFERRED;

    while (sent < bytes.size())
    {
        m_connectedFd->waitUntilReady(Readiness::Writable, deadline);

        const std::span<const uint8_t> source = std::span(bytes).subspan(sent);
        const SyscallResult<std::size_t> result = m_syscalls->sendSome(socketFd, source);
        if (result.hasFailed())
        {
            if (isRetryableTransferError(result.error))
            {
                continue;
            }
            throw std::system_error(result.error, "send");
        }

        sent += result.value;
    }
}

} // namespace adapter_socket
