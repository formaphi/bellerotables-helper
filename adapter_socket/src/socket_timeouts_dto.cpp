#include <adapter_socket/socket_endpoint/socket_timeouts_dto.h>
#include <chrono>
#include <stdexcept>

namespace adapter_socket {

constexpr std::chrono::milliseconds NO_TIMEOUT(0);

SocketTimeoutsDto SocketTimeoutsDto::create(std::chrono::milliseconds acceptTimeout,
                                            std::chrono::milliseconds connectionTimeout)
{
    const bool hasInvalidTimeout = acceptTimeout <= NO_TIMEOUT || connectionTimeout <= NO_TIMEOUT;
    if (hasInvalidTimeout)
    {
        throw std::invalid_argument("accept and connection timeouts must be greater than zero");
    }

    SocketTimeoutsDto timeouts;
    timeouts.acceptTimeout = acceptTimeout;
    timeouts.connectionTimeout = connectionTimeout;
    return timeouts;
}

} // namespace adapter_socket
