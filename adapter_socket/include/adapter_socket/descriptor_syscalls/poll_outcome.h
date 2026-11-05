#ifndef ADAPTER_SOCKET_POLL_OUTCOME_H
#define ADAPTER_SOCKET_POLL_OUTCOME_H

#include <cstdint>

namespace adapter_socket {

enum class PollOutcome : uint8_t
{
    Ready,
    TimedOut
};

} // namespace adapter_socket

#endif /* ADAPTER_SOCKET_POLL_OUTCOME_H */
