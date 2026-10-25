#if !defined(INTERFACE_FRAMED_CONNECTION_H)
#define INTERFACE_FRAMED_CONNECTION_H

#include <cstdint>
#include <vector>

namespace adapter_socket {

class IFramedConnection
{
public:
    virtual ~IFramedConnection() = default;

    virtual std::vector<uint8_t> readFrame() = 0;
    virtual void writeFrame(const std::vector<uint8_t>& payload) = 0;
};

} // namespace adapter_socket

#endif // INTERFACE_FRAMED_CONNECTION_H
