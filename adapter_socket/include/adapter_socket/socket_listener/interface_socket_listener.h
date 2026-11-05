#if !defined(INTERFACE_SOCKET_LISTENER_H)
#define INTERFACE_SOCKET_LISTENER_H

#include "adapter_socket/deadline/interface_deadline.h"
#include "adapter_socket/socket_listener/accepted_connection_dto.h"

namespace adapter_socket {

class ISocketListener
{
public:
    virtual ~ISocketListener() = default;

    virtual AcceptedConnectionDto acceptOne(const IDeadline& deadline) = 0;
};

} // namespace adapter_socket

#endif // INTERFACE_SOCKET_LISTENER_H
