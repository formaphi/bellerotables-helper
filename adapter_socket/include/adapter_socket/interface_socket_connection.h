#if !defined(INTERFACE_SOCKET_CONNECTION_H)
#define INTERFACE_SOCKET_CONNECTION_H

#include <adapter_socket/peer_credentials.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace adapter_socket {

class ISocketConnection
{
public:
    virtual ~ISocketConnection() = default;

    virtual std::vector<uint8_t> readExactly(std::size_t byteCount) = 0;
    virtual void writeAll(const std::vector<uint8_t>& bytes) = 0;
    virtual PeerCredentials peerCredentials() const = 0;
};

} // namespace adapter_socket

#endif // INTERFACE_SOCKET_CONNECTION_H
