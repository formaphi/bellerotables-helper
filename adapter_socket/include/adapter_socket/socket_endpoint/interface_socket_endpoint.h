#if !defined(INTERFACE_SOCKET_ENDPOINT_H)
#define INTERFACE_SOCKET_ENDPOINT_H

#include "adapter_socket/framed_connection/interface_framed_connection.h"

#include <memory>
#include <optional>

namespace adapter_socket {

class ISocketEndpoint
{
public:
    virtual ~ISocketEndpoint() = default;

    virtual std::optional<std::unique_ptr<IFramedConnection>> acceptAuthorized() = 0;
};

} // namespace adapter_socket

#endif // INTERFACE_SOCKET_ENDPOINT_H
