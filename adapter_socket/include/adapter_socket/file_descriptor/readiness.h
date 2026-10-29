#ifndef ADAPTER_SOCKET_READINESS_H
#define ADAPTER_SOCKET_READINESS_H

#include <cstdint>

namespace adapter_socket {

enum class Readiness : uint8_t
{
    Readable,
    Writable,
    Acceptable
};

} // namespace adapter_socket

#endif /* ADAPTER_SOCKET_READINESS_H */
