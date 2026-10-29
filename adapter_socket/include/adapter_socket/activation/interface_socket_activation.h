#if !defined(INTERFACE_SOCKET_ACTIVATION_H)
#define INTERFACE_SOCKET_ACTIVATION_H

#include <adapter_socket/file_descriptor/interface_file_descriptor.h>

#include <memory>

namespace adapter_socket {

class ISocketActivation
{
public:
    virtual ~ISocketActivation() = default;

    [[nodiscard]] virtual std::unique_ptr<IFileDescriptor> takeListeningFd() const = 0;
};

} // namespace adapter_socket

#endif // INTERFACE_SOCKET_ACTIVATION_H
