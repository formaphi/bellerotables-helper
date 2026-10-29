#include <adapter_socket/framed_connection/frame_header_dto.h>

#include <adapter_socket/exceptions/socket_exception.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <vector>

constexpr uint32_t SAMPLE_PAYLOAD_LENGTH = 258;
constexpr uint8_t ZERO_FILL_BYTE = 0;
constexpr std::size_t TOO_SHORT_HEADER_BYTES = 3;
constexpr std::size_t TOO_LONG_HEADER_BYTES = 5;

const std::vector<uint8_t> HEADER_FOR_258_BYTES{0x00, 0x00, 0x01, 0x02};
const std::vector<uint8_t> HEADER_FOR_MAXIMUM_PAYLOAD{0x00, 0x01, 0x00, 0x00};
const std::vector<uint8_t> HEADER_ONE_ABOVE_MAXIMUM{0x00, 0x01, 0x00, 0x01};

TEST(FrameHeaderDtoTest, ToBytesRoundTripsThroughParse)
{
    adapter_socket::FrameHeaderDto original;
    original.payloadLength = SAMPLE_PAYLOAD_LENGTH;

    const std::vector<uint8_t> encoded = original.toBytes();
    const adapter_socket::FrameHeaderDto parsed = adapter_socket::FrameHeaderDto::parse(encoded);

    EXPECT_EQ(parsed.payloadLength, SAMPLE_PAYLOAD_LENGTH);
}

TEST(FrameHeaderDtoTest, ToBytesEncodesLengthBigEndian)
{
    adapter_socket::FrameHeaderDto header;
    header.payloadLength = SAMPLE_PAYLOAD_LENGTH;

    EXPECT_EQ(header.toBytes(), HEADER_FOR_258_BYTES);
}

TEST(FrameHeaderDtoTest, ParseDecodesBigEndianBytes)
{
    const adapter_socket::FrameHeaderDto parsed =
        adapter_socket::FrameHeaderDto::parse(HEADER_FOR_258_BYTES);

    EXPECT_EQ(parsed.payloadLength, SAMPLE_PAYLOAD_LENGTH);
}

TEST(FrameHeaderDtoTest, WrongHeaderSizeThrowsConnectionClosed)
{
    const std::vector<uint8_t> tooShort(TOO_SHORT_HEADER_BYTES, ZERO_FILL_BYTE);
    const std::vector<uint8_t> tooLong(TOO_LONG_HEADER_BYTES, ZERO_FILL_BYTE);

    EXPECT_THROW(static_cast<void>(adapter_socket::FrameHeaderDto::parse(tooShort)),
                 adapter_socket::ConnectionClosedException);
    EXPECT_THROW(static_cast<void>(adapter_socket::FrameHeaderDto::parse(tooLong)),
                 adapter_socket::ConnectionClosedException);
}

TEST(FrameHeaderDtoTest, MaximumPayloadLengthIsAccepted)
{
    const adapter_socket::FrameHeaderDto parsed =
        adapter_socket::FrameHeaderDto::parse(HEADER_FOR_MAXIMUM_PAYLOAD);

    EXPECT_EQ(parsed.payloadLength, adapter_socket::FrameHeaderDto::MAX_PAYLOAD_BYTES);
}

TEST(FrameHeaderDtoTest, LengthAboveMaximumThrowsFrameTooLarge)
{
    EXPECT_THROW(static_cast<void>(adapter_socket::FrameHeaderDto::parse(HEADER_ONE_ABOVE_MAXIMUM)),
                 adapter_socket::FrameTooLargeException);
}
