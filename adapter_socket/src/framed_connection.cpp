#include <adapter_socket/framed_connection.h>

#include <adapter_socket/exceptions/socket_exception.h>

namespace adapter_socket {

constexpr uint32_t EMPTY_PAYLOAD_BYTES = 0;
constexpr std::size_t MOST_SIGNIFICANT_BYTE_INDEX = 0;
constexpr std::size_t UPPER_MIDDLE_BYTE_INDEX = 1;
constexpr std::size_t LOWER_MIDDLE_BYTE_INDEX = 2;
constexpr std::size_t LEAST_SIGNIFICANT_BYTE_INDEX = 3;
constexpr unsigned MOST_SIGNIFICANT_BYTE_SHIFT = 24;
constexpr unsigned UPPER_MIDDLE_BYTE_SHIFT = 16;
constexpr unsigned LOWER_MIDDLE_BYTE_SHIFT = 8;

FramedConnection::FramedConnection(std::shared_ptr<ISocketConnection> connection)
    : m_connection(connection)
{
}

std::vector<uint8_t> FramedConnection::readFrame()
{
    const std::vector<uint8_t> header = m_connection->readExactly(HEADER_BYTES);
    if (header.size() != HEADER_BYTES)
    {
        throw ConnectionClosedException("frame header ended before all header bytes arrived");
    }

    const uint32_t mostSignificantPart = static_cast<uint32_t>(header[MOST_SIGNIFICANT_BYTE_INDEX]) << MOST_SIGNIFICANT_BYTE_SHIFT;
    const uint32_t upperMiddlePart = static_cast<uint32_t>(header[UPPER_MIDDLE_BYTE_INDEX]) << UPPER_MIDDLE_BYTE_SHIFT;
    const uint32_t lowerMiddlePart = static_cast<uint32_t>(header[LOWER_MIDDLE_BYTE_INDEX]) << LOWER_MIDDLE_BYTE_SHIFT;
    const uint32_t leastSignificantPart = static_cast<uint32_t>(header[LEAST_SIGNIFICANT_BYTE_INDEX]);
    const uint32_t declaredLength = mostSignificantPart | upperMiddlePart | lowerMiddlePart | leastSignificantPart;

    if (declaredLength > MAX_PAYLOAD_BYTES)
    {
        throw FrameTooLargeException("declared frame length exceeds the maximum payload size");
    }

    if (declaredLength == EMPTY_PAYLOAD_BYTES)
    {
        return std::vector<uint8_t>();
    }

    return m_connection->readExactly(static_cast<std::size_t>(declaredLength));
}

void FramedConnection::writeFrame(const std::vector<uint8_t>& payload)
{
    if (payload.size() > MAX_PAYLOAD_BYTES)
    {
        throw FrameTooLargeException("frame payload exceeds the maximum payload size");
    }

    const auto length = static_cast<uint32_t>(payload.size());
    const uint8_t mostSignificantByte = static_cast<uint8_t>(length >> MOST_SIGNIFICANT_BYTE_SHIFT);
    const uint8_t upperMiddleByte = static_cast<uint8_t>(length >> UPPER_MIDDLE_BYTE_SHIFT);
    const uint8_t lowerMiddleByte = static_cast<uint8_t>(length >> LOWER_MIDDLE_BYTE_SHIFT);
    const uint8_t leastSignificantByte = static_cast<uint8_t>(length);
    const std::size_t frameBytes = HEADER_BYTES + payload.size();

    std::vector<uint8_t> frame;
    frame.reserve(frameBytes);
    frame.push_back(mostSignificantByte);
    frame.push_back(upperMiddleByte);
    frame.push_back(lowerMiddleByte);
    frame.push_back(leastSignificantByte);
    frame.insert(frame.end(), payload.begin(), payload.end());

    m_connection->writeAll(frame);
}

PeerCredentials FramedConnection::peerCredentials() const
{
    return m_connection->peerCredentials();
}

} // namespace adapter_socket
