#include "adapter_socket/framed_connection/framed_connection.h"

#include "adapter_socket/exceptions/socket_exception.h"
#include "adapter_socket/framed_connection/frame_header_dto.h"

#include <utility>

namespace adapter_socket {

FramedConnection::FramedConnection(std::unique_ptr<ISocketConnection> connection,
                                   std::shared_ptr<IDeadlineFactory> deadlineFactory,
                                   std::chrono::milliseconds frameTimeout)
    : m_connection(std::move(connection))
    , m_deadlineFactory(std::move(deadlineFactory))
    , m_frameTimeout(frameTimeout)
{
}

std::vector<uint8_t> FramedConnection::readFrame()
{
    const std::unique_ptr<IDeadline> deadline = m_deadlineFactory->startingNow(m_frameTimeout);
    const std::vector<uint8_t> headerBytes =
        m_connection->readExactly(FrameHeaderDto::HEADER_BYTES, *deadline);
    const FrameHeaderDto header = FrameHeaderDto::parse(headerBytes);
    if (header.payloadLength == FrameHeaderDto::EMPTY_PAYLOAD_BYTES)
    {
        return {};
    }

    return m_connection->readExactly(static_cast<std::size_t>(header.payloadLength), *deadline);
}

void FramedConnection::writeFrame(const std::vector<uint8_t>& payload)
{
    const bool isTooLarge = payload.size() > FrameHeaderDto::MAX_PAYLOAD_BYTES;
    if (isTooLarge)
    {
        throw FrameTooLargeException("frame payload exceeds the maximum payload size");
    }

    FrameHeaderDto header;
    header.payloadLength = static_cast<uint32_t>(payload.size());
    std::vector<uint8_t> frame = header.toBytes();
    frame.insert(frame.end(), payload.begin(), payload.end());

    const std::unique_ptr<IDeadline> deadline = m_deadlineFactory->startingNow(m_frameTimeout);
    m_connection->writeAll(frame, *deadline);
}

} // namespace adapter_socket
