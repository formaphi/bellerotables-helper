#include "adapter_socket/framed_connection/frame_header_dto.h"

#include "adapter_socket/exceptions/socket_exception.h"
#include "parameterized_case.h"

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
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

TEST(FrameHeaderDtoTest, LengthAboveMaximumThrowsFrameTooLarge)
{
    EXPECT_THROW(static_cast<void>(adapter_socket::FrameHeaderDto::parse(HEADER_ONE_ABOVE_MAXIMUM)),
                 adapter_socket::FrameTooLargeException);
}

struct ValidHeaderCase
{
    std::vector<uint8_t> bytes;
    uint32_t expectedPayloadLength;
    const char* name;
};

struct WrongSizeHeaderCase
{
    std::size_t byteCount;
    const char* name;
};

const std::vector<ValidHeaderCase> VALID_HEADERS{
    {HEADER_FOR_258_BYTES, SAMPLE_PAYLOAD_LENGTH, "BigEndian258"},
    {HEADER_FOR_MAXIMUM_PAYLOAD,
     static_cast<uint32_t>(adapter_socket::FrameHeaderDto::MAX_PAYLOAD_BYTES),
     "MaximumPayloadLength"},
};

const std::vector<WrongSizeHeaderCase> WRONG_SIZE_HEADERS{
    {TOO_SHORT_HEADER_BYTES, "TooShort"},
    {TOO_LONG_HEADER_BYTES, "TooLong"},
};

class FrameHeaderDtoParseTest : public ::testing::TestWithParam<ValidHeaderCase>
{
};

class FrameHeaderDtoWrongSizeTest : public ::testing::TestWithParam<WrongSizeHeaderCase>
{
};

TEST_P(FrameHeaderDtoParseTest, DecodesPayloadLength)
{
    const adapter_socket::FrameHeaderDto parsed =
        adapter_socket::FrameHeaderDto::parse(GetParam().bytes);

    EXPECT_EQ(parsed.payloadLength, GetParam().expectedPayloadLength);
}

TEST_P(FrameHeaderDtoWrongSizeTest, ThrowsConnectionClosed)
{
    const std::vector<uint8_t> header(GetParam().byteCount, ZERO_FILL_BYTE);

    EXPECT_THROW(static_cast<void>(adapter_socket::FrameHeaderDto::parse(header)),
                 adapter_socket::ConnectionClosedException);
}

INSTANTIATE_TEST_SUITE_P(Header, FrameHeaderDtoParseTest, ::testing::ValuesIn(VALID_HEADERS),
                         caseName<ValidHeaderCase>);

INSTANTIATE_TEST_SUITE_P(Header, FrameHeaderDtoWrongSizeTest,
                         ::testing::ValuesIn(WRONG_SIZE_HEADERS), caseName<WrongSizeHeaderCase>);
