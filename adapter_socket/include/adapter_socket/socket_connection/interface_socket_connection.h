#if !defined(INTERFACE_SOCKET_CONNECTION_H)
#define INTERFACE_SOCKET_CONNECTION_H

#include <adapter_socket/deadline/interface_deadline.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace adapter_socket {

class ISocketConnection
{
public:
    virtual ~ISocketConnection() = default;

    virtual std::vector<uint8_t> readExactly(std::size_t byteCount, const IDeadline& deadline) = 0;
    virtual void writeAll(const std::vector<uint8_t>& bytes, const IDeadline& deadline) = 0;
};

} // namespace adapter_socket

#endif // INTERFACE_SOCKET_CONNECTION_H
