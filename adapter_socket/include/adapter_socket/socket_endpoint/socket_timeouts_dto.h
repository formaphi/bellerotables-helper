#ifndef ADAPTER_SOCKET_SOCKET_TIMEOUTS_DTO_H
#define ADAPTER_SOCKET_SOCKET_TIMEOUTS_DTO_H

#include <chrono>

namespace adapter_socket {

struct SocketTimeoutsDto
{
    static constexpr std::chrono::milliseconds DEFAULT_ACCEPT_TIMEOUT =
        std::chrono::milliseconds(5000);
    static constexpr std::chrono::milliseconds DEFAULT_CONNECTION_TIMEOUT =
        std::chrono::milliseconds(5000);

    std::chrono::milliseconds acceptTimeout = DEFAULT_ACCEPT_TIMEOUT;
    std::chrono::milliseconds connectionTimeout = DEFAULT_CONNECTION_TIMEOUT;

    [[nodiscard]] static SocketTimeoutsDto create(std::chrono::milliseconds acceptTimeout,
                                                  std::chrono::milliseconds connectionTimeout);
};

} // namespace adapter_socket

#endif // ADAPTER_SOCKET_SOCKET_TIMEOUTS_DTO_H
