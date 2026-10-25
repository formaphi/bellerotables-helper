#if !defined(INTERFACE_FILE_DESCRIPTOR_H)
#define INTERFACE_FILE_DESCRIPTOR_H

#include <adapter_socket/deadline/interface_deadline.h>
#include <adapter_socket/file_descriptor/readiness.h>

namespace adapter_socket {

class IFileDescriptor
{
public:
    virtual ~IFileDescriptor() = default;

    [[nodiscard]] virtual int value() const = 0;
    virtual void waitUntilReady(Readiness readiness, const IDeadline& deadline) const = 0;
};

} // namespace adapter_socket

#endif // INTERFACE_FILE_DESCRIPTOR_H
