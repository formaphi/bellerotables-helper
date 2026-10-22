#ifndef ADAPTER_SOCKET_UNIX_SOCKET_CONNECTION_H
#define ADAPTER_SOCKET_UNIX_SOCKET_CONNECTION_H

#include <adapter_socket/interface_socket_connection.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace adapter_socket {

class UnixSocketConnection final : public ISocketConnection
{
public:
    static constexpr std::chrono::milliseconds DEFAULT_TIMEOUT = std::chrono::milliseconds(5000);

    explicit UnixSocketConnection(int connectedFd, std::chrono::milliseconds timeout = DEFAULT_TIMEOUT);
    ~UnixSocketConnection() override;

    UnixSocketConnection(const UnixSocketConnection&) = delete;
    UnixSocketConnection& operator=(const UnixSocketConnection&) = delete;

    std::vector<uint8_t> readExactly(std::size_t byteCount) override;
    void writeAll(const std::vector<uint8_t>& bytes) override;
    PeerCredentials peerCredentials() const override;

private:
    static constexpr int INVALID_FD = -1;

    void configureTimeouts(std::chrono::milliseconds timeout);
    void waitUntilReady(short pollEvents, const std::chrono::steady_clock::time_point& deadline, const char* operation) const;
    PeerCredentials readPeerCredentials() const;
    void closeFd() noexcept;

    int m_fd;
    std::chrono::milliseconds m_timeout;
    PeerCredentials m_peerCredentials;
};

} // namespace adapter_socket

#endif /* ADAPTER_SOCKET_UNIX_SOCKET_CONNECTION_H */
