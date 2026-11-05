#include "adapter_socket/framed_connection/frame_header_dto.h"

#include "adapter_socket/exceptions/socket_exception.h"

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

    /* LEARN: a frame is a 4-byte header followed by the payload.

         header (4 bytes = 32 bits)                   payload
       +--------+--------+--------+--------+---------------------------+
       | byte 0 | byte 1 | byte 2 | byte 3 |  N bytes, N = the length  |
       +--------+--------+--------+--------+---------------------------+
       | bits   | bits   | bits   | bits   |  at most MAX_PAYLOAD_BYTES|
       | 31..24 | 23..16 | 15..8  | 7..0   |                           |
       +--------+--------+--------+--------+---------------------------+
         most significant byte first          (length 258 = 00 00 01 02)

       The header is one unsigned 32-bit integer in big-endian (network) order holding the
       payload length only, never the header's own 4 bytes. Length 258 is 1 * 256 + 2. A length
       of 0 is a valid empty payload.
       The bytes are assembled with shifts instead of memcpy or ntohl, so the result does not
       depend on the host byte order or on the buffer alignment. Each byte is widened to
       uint32_t before shifting: as a plain int, 0xFF << 24 would overflow.
       The declared length is checked against MAX_PAYLOAD_BYTES before any payload is read, so
       a peer cannot make the helper allocate or wait for an arbitrarily large frame.
    */
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
    /* LEARN: the reverse of parse: payloadLength (32 bits) is split into the 4 header bytes,
       most significant byte first (big-endian). Each byte is the length shifted right until its
       bits reach the lowest 8, then cut to uint8_t, which keeps only those 8 bits.

         payloadLength = 258 = 0x00000102

         payloadLength >> 24  ->  0x00000000  ->  byte 0 = 00   (bits 31..24)
         payloadLength >> 16  ->  0x00000000  ->  byte 1 = 00   (bits 23..16)
         payloadLength >>  8  ->  0x00000001  ->  byte 2 = 01   (bits 15..8)
         payloadLength        ->  0x00000102  ->  byte 3 = 02   (bits 7..0)

       header = { 00, 00, 01, 02 }
       The last byte needs no shift: the cast alone drops everything above bit 7. The shift
       constants are the same ones parse uses, so the two directions cannot drift apart.
    */
    const auto mostSignificantByte =
        static_cast<uint8_t>(payloadLength >> MOST_SIGNIFICANT_BYTE_SHIFT);
    const auto upperMiddleByte = static_cast<uint8_t>(payloadLength >> UPPER_MIDDLE_BYTE_SHIFT);
    const auto lowerMiddleByte = static_cast<uint8_t>(payloadLength >> LOWER_MIDDLE_BYTE_SHIFT);
    const auto leastSignificantByte = static_cast<uint8_t>(payloadLength);

    return {mostSignificantByte, upperMiddleByte, lowerMiddleByte, leastSignificantByte};
}

} // namespace adapter_socket
