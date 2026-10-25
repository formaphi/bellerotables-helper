#include <adapter_socket/framed_connection/frame_header_dto.h>

#include <adapter_socket/exceptions/socket_exception.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace adapter_socket {

constexpr std::size_t MOST_SIGNIFICANT_BYTE_INDEX = 0;
constexpr std::size_t UPPER_MIDDLE_BYTE_INDEX = 1;
constexpr std::size_t LOWER_MIDDLE_BYTE_INDEX = 2;
constexpr std::size_t LEAST_SIGNIFICANT_BYTE_INDEX = 3;
constexpr unsigned MOST_SIGNIFICANT_BYTE_SHIFT = 24;
constexpr unsigned UPPER_MIDDLE_BYTE_SHIFT = 16;
constexpr unsigned LOWER_MIDDLE_BYTE_SHIFT = 8;

FrameHeaderDto FrameHeaderDto::parse(const std::vector<uint8_t>& headerBytes)
{
    const bool isTruncated = headerBytes.size() != HEADER_BYTES;
    if (isTruncated)
    {
        throw ConnectionClosedException("frame header ended before all header bytes arrived");
    }

    const uint32_t mostSignificantPart =
        static_cast<uint32_t>(headerBytes[MOST_SIGNIFICANT_BYTE_INDEX])
        << MOST_SIGNIFICANT_BYTE_SHIFT;
    const uint32_t upperMiddlePart = static_cast<uint32_t>(headerBytes[UPPER_MIDDLE_BYTE_INDEX])
                                     << UPPER_MIDDLE_BYTE_SHIFT;
    const uint32_t lowerMiddlePart = static_cast<uint32_t>(headerBytes[LOWER_MIDDLE_BYTE_INDEX])
                                     << LOWER_MIDDLE_BYTE_SHIFT;
    const auto leastSignificantPart =
        static_cast<uint32_t>(headerBytes[LEAST_SIGNIFICANT_BYTE_INDEX]);
    const uint32_t declaredLength =
        mostSignificantPart | upperMiddlePart | lowerMiddlePart | leastSignificantPart;

    const bool isTooLarge = declaredLength > MAX_PAYLOAD_BYTES;
    if (isTooLarge)
    {
        throw FrameTooLargeException("declared frame length exceeds the maximum payload size");
    }

    FrameHeaderDto header;
    header.payloadLength = declaredLength;
    return header;
}

std::vector<uint8_t> FrameHeaderDto::toBytes() const
{
    const auto mostSignificantByte =
        static_cast<uint8_t>(payloadLength >> MOST_SIGNIFICANT_BYTE_SHIFT);
    const auto upperMiddleByte = static_cast<uint8_t>(payloadLength >> UPPER_MIDDLE_BYTE_SHIFT);
    const auto lowerMiddleByte = static_cast<uint8_t>(payloadLength >> LOWER_MIDDLE_BYTE_SHIFT);
    const auto leastSignificantByte = static_cast<uint8_t>(payloadLength);

    return {mostSignificantByte, upperMiddleByte, lowerMiddleByte, leastSignificantByte};
}

} // namespace adapter_socket
