#ifndef ADAPTER_SOCKET_UNIX_SOCKET_CONNECTION_H
#define ADAPTER_SOCKET_UNIX_SOCKET_CONNECTION_H

#include "adapter_socket/deadline/interface_deadline.h"
#include "adapter_socket/file_descriptor/interface_file_descriptor.h"
#include "adapter_socket/socket_connection/interface_socket_connection.h"
#include "adapter_socket/socket_syscalls/interface_socket_syscalls.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace adapter_socket {

class UnixSocketConnection final : public ISocketConnection
{
public:
    UnixSocketConnection(std::unique_ptr<IFileDescriptor> connectedFd,
                         std::shared_ptr<ISocketSyscalls> syscalls);
    ~UnixSocketConnection() override = default;

    UnixSocketConnection(const UnixSocketConnection&) = delete;
    UnixSocketConnection& operator=(const UnixSocketConnection&) = delete;
    UnixSocketConnection(UnixSocketConnection&&) = delete;
    UnixSocketConnection& operator=(UnixSocketConnection&&) = delete;

    std::vector<uint8_t> readExactly(std::size_t byteCount, const IDeadline& deadline) override;
    void writeAll(const std::vector<uint8_t>& bytes, const IDeadline& deadline) override;

private:
    std::unique_ptr<IFileDescriptor> m_connectedFd;
    std::shared_ptr<ISocketSyscalls> m_syscalls;
};

} // namespace adapter_socket

#endif /* ADAPTER_SOCKET_UNIX_SOCKET_CONNECTION_H */
