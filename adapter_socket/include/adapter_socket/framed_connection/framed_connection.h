#ifndef ADAPTER_SOCKET_FRAMED_CONNECTION_H
#define ADAPTER_SOCKET_FRAMED_CONNECTION_H

#include "adapter_socket/deadline/interface_deadline_factory.h"
#include "adapter_socket/framed_connection/interface_framed_connection.h"
#include "adapter_socket/socket_connection/interface_socket_connection.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

namespace adapter_socket {

class FramedConnection final : public IFramedConnection
{
public:
    FramedConnection(std::unique_ptr<ISocketConnection> connection,
                     std::shared_ptr<IDeadlineFactory> deadlineFactory,
                     std::chrono::milliseconds frameTimeout);
    ~FramedConnection() override = default;

    FramedConnection(const FramedConnection&) = delete;
    FramedConnection& operator=(const FramedConnection&) = delete;
    FramedConnection(FramedConnection&&) = delete;
    FramedConnection& operator=(FramedConnection&&) = delete;

    std::vector<uint8_t> readFrame() override;
    void writeFrame(const std::vector<uint8_t>& payload) override;

private:
    std::unique_ptr<ISocketConnection> m_connection;
    std::shared_ptr<IDeadlineFactory> m_deadlineFactory;
    std::chrono::milliseconds m_frameTimeout;
};

} // namespace adapter_socket

#endif /* ADAPTER_SOCKET_FRAMED_CONNECTION_H */
