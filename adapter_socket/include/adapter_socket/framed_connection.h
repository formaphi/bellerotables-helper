#ifndef ADAPTER_SOCKET_FRAMED_CONNECTION_H
#define ADAPTER_SOCKET_FRAMED_CONNECTION_H

#include <adapter_socket/interface_socket_connection.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace adapter_socket {

class FramedConnection final
{
public:
    static constexpr std::size_t HEADER_BYTES = 4;
    static constexpr std::size_t MAX_PAYLOAD_BYTES = 65536;

    explicit FramedConnection(std::shared_ptr<ISocketConnection> connection);
    ~FramedConnection() = default;

    std::vector<uint8_t> readFrame();
    void writeFrame(const std::vector<uint8_t>& payload);
    PeerCredentials peerCredentials() const;

private:
    std::shared_ptr<ISocketConnection> m_connection;
};

} // namespace adapter_socket

#endif /* ADAPTER_SOCKET_FRAMED_CONNECTION_H */
