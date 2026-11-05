#include "adapter_socket/framed_connection/framed_connection.h"

#include "adapter_socket/deadline/interface_deadline.h"
#include "adapter_socket/deadline/interface_deadline_factory.h"
#include "adapter_socket/deadline/steady_deadline_factory.h"
#include "adapter_socket/exceptions/socket_exception.h"
#include "adapter_socket/framed_connection/frame_header_dto.h"
#include "adapter_socket/mock/mock_deadline.h"
#include "adapter_socket/mock/mock_deadline_factory.h"
#include "adapter_socket/socket_connection/interface_socket_connection.h"
#include "parameterized_case.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

using adapter_socket::MockDeadline;
using adapter_socket::MockDeadlineFactory;
using ::testing::_;

constexpr std::size_t INCOMING_START_POSITION = 0;
constexpr std::size_t TRUNCATED_HEADER_BYTES = 2;
constexpr std::size_t ONE_BYTE_CHUNK_COUNT = 7;
constexpr uint8_t ZERO_FILL_BYTE = 0;
constexpr std::size_t WHOLE_FRAME_CHUNK_BYTES = 1024;
constexpr std::size_t ONE_BYTE_CHUNK_BYTES = 1;
constexpr std::size_t TWO_BYTE_LENGTH_PAYLOAD_BYTES = 258;
constexpr uint8_t PAYLOAD_FILL_BYTE = 0xAB;
constexpr std::size_t BYTES_ABOVE_MAXIMUM = 1;
constexpr std::chrono::milliseconds FRAME_TIMEOUT(5000);

const std::vector<uint8_t> HEADER_FOR_258_BYTES{0x00, 0x00, 0x01, 0x02};
const std::vector<uint8_t> HEADER_FOR_EMPTY_PAYLOAD{0x00, 0x00, 0x00, 0x00};
const std::vector<uint8_t> HEADER_ABOVE_MAXIMUM{0x00, 0x01, 0x00, 0x01};
const std::vector<uint8_t> THREE_BYTE_PAYLOAD{9, 8, 7};
const std::vector<uint8_t> THREE_BYTE_FRAME{0x00, 0x00, 0x00, 0x03, 9, 8, 7};
const std::vector<uint8_t> FRAME_CUT_INSIDE_PAYLOAD{0x00, 0x00, 0x00, 0x05, 1, 2};
const std::shared_ptr<adapter_socket::IDeadlineFactory> STEADY_DEADLINES =
    std::make_shared<adapter_socket::SteadyDeadlineFactory>();

class FakeSocketConnection final : public adapter_socket::ISocketConnection
{
public:
    FakeSocketConnection(std::vector<uint8_t> incoming, std::size_t chunkBytes)
        : m_incoming(incoming)
        , m_chunkBytes(chunkBytes)
        , m_readPosition(INCOMING_START_POSITION)
    {
    }

    std::vector<uint8_t> readExactly(std::size_t byteCount,
                                     const adapter_socket::IDeadline&) override
    {
        m_readRequests.push_back(byteCount);

        std::vector<uint8_t> bytes;
        while (bytes.size() < byteCount)
        {
            if (m_readPosition >= m_incoming.size())
            {
                throw adapter_socket::ConnectionClosedException("fake connection ended");
            }

            const std::size_t available = m_incoming.size() - m_readPosition;
            const std::size_t stillNeeded = byteCount - bytes.size();
            const std::size_t step = std::min({m_chunkBytes, available, stillNeeded});

            const auto chunkFirst =
                m_incoming.begin() + static_cast<std::ptrdiff_t>(m_readPosition);
            const auto chunkLast = chunkFirst + static_cast<std::ptrdiff_t>(step);
            bytes.insert(bytes.end(), chunkFirst, chunkLast);
            m_readPosition += step;
            m_deliveredChunkSizes.push_back(step);
        }

        return bytes;
    }

    void writeAll(const std::vector<uint8_t>& bytes, const adapter_socket::IDeadline&) override
    {
        m_written.insert(m_written.end(), bytes.begin(), bytes.end());
    }

    const std::vector<uint8_t>& written() const
    {
        return m_written;
    }

    const std::vector<std::size_t>& readRequests() const
    {
        return m_readRequests;
    }

    const std::vector<std::size_t>& deliveredChunkSizes() const
    {
        return m_deliveredChunkSizes;
    }

private:
    std::vector<uint8_t> m_incoming;
    std::size_t m_chunkBytes;
    std::size_t m_readPosition;
    std::vector<uint8_t> m_written;
    std::vector<std::size_t> m_readRequests;
    std::vector<std::size_t> m_deliveredChunkSizes;
};

class TruncatedHeaderConnection final : public adapter_socket::ISocketConnection
{
public:
    std::vector<uint8_t> readExactly(std::size_t, const adapter_socket::IDeadline&) override
    {
        return std::vector<uint8_t>(TRUNCATED_HEADER_BYTES, ZERO_FILL_BYTE);
    }

    void writeAll(const std::vector<uint8_t>&, const adapter_socket::IDeadline&) override {}
};

TEST(FramedConnectionTest, DeclaredLengthAboveMaximumThrowsWithoutReadingPayload)
{
    auto ownedReaderSide =
        std::make_unique<FakeSocketConnection>(HEADER_ABOVE_MAXIMUM, WHOLE_FRAME_CHUNK_BYTES);
    const FakeSocketConnection* const readerSide = ownedReaderSide.get();
    adapter_socket::FramedConnection reader(std::move(ownedReaderSide), STEADY_DEADLINES,
                                            FRAME_TIMEOUT);

    EXPECT_THROW(reader.readFrame(), adapter_socket::FrameTooLargeException);
    EXPECT_EQ(readerSide->readRequests(),
              (std::vector<std::size_t>{adapter_socket::FrameHeaderDto::HEADER_BYTES}));
}

TEST(FramedConnectionTest, WriteFrameAboveMaximumThrowsAndWritesNothing)
{
    auto ownedWriterSide =
        std::make_unique<FakeSocketConnection>(std::vector<uint8_t>(), WHOLE_FRAME_CHUNK_BYTES);
    const FakeSocketConnection* const writerSide = ownedWriterSide.get();
    adapter_socket::FramedConnection writer(std::move(ownedWriterSide), STEADY_DEADLINES,
                                            FRAME_TIMEOUT);
    const std::size_t tooLargeBytes =
        adapter_socket::FrameHeaderDto::MAX_PAYLOAD_BYTES + BYTES_ABOVE_MAXIMUM;
    const std::vector<uint8_t> tooLarge(tooLargeBytes, ZERO_FILL_BYTE);

    EXPECT_THROW(writer.writeFrame(tooLarge), adapter_socket::FrameTooLargeException);
    EXPECT_TRUE(writerSide->written().empty());
}

TEST(FramedConnectionTest, FrameDeliveredOneByteAtATimeIsAssembled)
{
    auto ownedReaderSide =
        std::make_unique<FakeSocketConnection>(THREE_BYTE_FRAME, ONE_BYTE_CHUNK_BYTES);
    const FakeSocketConnection* const readerSide = ownedReaderSide.get();
    adapter_socket::FramedConnection reader(std::move(ownedReaderSide), STEADY_DEADLINES,
                                            FRAME_TIMEOUT);

    EXPECT_EQ(reader.readFrame(), THREE_BYTE_PAYLOAD);
    EXPECT_EQ(readerSide->deliveredChunkSizes(),
              std::vector<std::size_t>(ONE_BYTE_CHUNK_COUNT, ONE_BYTE_CHUNK_BYTES));
}

TEST(FramedConnectionTest, EndOfStreamInsidePayloadThrowsConnectionClosed)
{
    auto ownedReaderSide =
        std::make_unique<FakeSocketConnection>(FRAME_CUT_INSIDE_PAYLOAD, WHOLE_FRAME_CHUNK_BYTES);
    adapter_socket::FramedConnection reader(std::move(ownedReaderSide), STEADY_DEADLINES,
                                            FRAME_TIMEOUT);

    EXPECT_THROW(reader.readFrame(), adapter_socket::ConnectionClosedException);
}

TEST(FramedConnectionTest, ShortHeaderThrowsConnectionClosedWithoutIndexingPastEnd)
{
    adapter_socket::FramedConnection reader(std::make_unique<TruncatedHeaderConnection>(),
                                            STEADY_DEADLINES, FRAME_TIMEOUT);

    EXPECT_THROW(reader.readFrame(), adapter_socket::ConnectionClosedException);
}

TEST(FramedConnectionTest, ReadFrameOpensOneDeadlineForHeaderAndPayload)
{
    auto ownedReaderSide =
        std::make_unique<FakeSocketConnection>(THREE_BYTE_FRAME, WHOLE_FRAME_CHUNK_BYTES);
    const auto deadlines = std::make_shared<MockDeadlineFactory>();
    EXPECT_CALL(*deadlines, startingNow(FRAME_TIMEOUT))
        .Times(1)
        .WillOnce([](std::chrono::milliseconds) -> std::unique_ptr<adapter_socket::IDeadline> {
            return std::make_unique<MockDeadline>();
        });
    adapter_socket::FramedConnection reader(std::move(ownedReaderSide), deadlines, FRAME_TIMEOUT);

    EXPECT_EQ(reader.readFrame(), THREE_BYTE_PAYLOAD);
}

TEST(FramedConnectionTest, WriteFrameOpensOneDeadline)
{
    auto ownedWriterSide =
        std::make_unique<FakeSocketConnection>(std::vector<uint8_t>(), WHOLE_FRAME_CHUNK_BYTES);
    const FakeSocketConnection* const writerSide = ownedWriterSide.get();
    const auto deadlines = std::make_shared<MockDeadlineFactory>();
    EXPECT_CALL(*deadlines, startingNow(FRAME_TIMEOUT))
        .Times(1)
        .WillOnce([](std::chrono::milliseconds) -> std::unique_ptr<adapter_socket::IDeadline> {
            return std::make_unique<MockDeadline>();
        });
    adapter_socket::FramedConnection writer(std::move(ownedWriterSide), deadlines, FRAME_TIMEOUT);

    writer.writeFrame(THREE_BYTE_PAYLOAD);

    EXPECT_EQ(writerSide->written(), THREE_BYTE_FRAME);
}

TEST(FramedConnectionTest, WriteFrameAboveMaximumOpensNoDeadline)
{
    auto ownedWriterSide =
        std::make_unique<FakeSocketConnection>(std::vector<uint8_t>(), WHOLE_FRAME_CHUNK_BYTES);
    const auto deadlines = std::make_shared<MockDeadlineFactory>();
    EXPECT_CALL(*deadlines, startingNow(_)).Times(0);
    adapter_socket::FramedConnection writer(std::move(ownedWriterSide), deadlines, FRAME_TIMEOUT);
    const std::size_t tooLargeBytes =
        adapter_socket::FrameHeaderDto::MAX_PAYLOAD_BYTES + BYTES_ABOVE_MAXIMUM;
    const std::vector<uint8_t> tooLarge(tooLargeBytes, ZERO_FILL_BYTE);

    EXPECT_THROW(writer.writeFrame(tooLarge), adapter_socket::FrameTooLargeException);
}

struct PayloadCase
{
    std::vector<uint8_t> payload;
    std::vector<uint8_t> expectedHeader;
    const char* name;
};

const std::vector<PayloadCase> PAYLOADS{
    {{'h', 'e', 'l', 'l', 'o'}, {0x00, 0x00, 0x00, 0x05}, "FiveBytes"},
    {std::vector<uint8_t>(TWO_BYTE_LENGTH_PAYLOAD_BYTES, PAYLOAD_FILL_BYTE), HEADER_FOR_258_BYTES,
     "LengthNeedingTwoBytes"},
    {{}, HEADER_FOR_EMPTY_PAYLOAD, "Empty"},
};

class FramedConnectionPayloadTest : public ::testing::TestWithParam<PayloadCase>
{
};

TEST_P(FramedConnectionPayloadTest, WriteFramePrefixesBigEndianLengthHeader)
{
    auto ownedWriterSide =
        std::make_unique<FakeSocketConnection>(std::vector<uint8_t>(), WHOLE_FRAME_CHUNK_BYTES);
    const FakeSocketConnection* const writerSide = ownedWriterSide.get();
    adapter_socket::FramedConnection writer(std::move(ownedWriterSide), STEADY_DEADLINES,
                                            FRAME_TIMEOUT);

    writer.writeFrame(GetParam().payload);

    const std::vector<uint8_t>& written = writerSide->written();
    const std::size_t expectedWrittenBytes =
        adapter_socket::FrameHeaderDto::HEADER_BYTES + GetParam().payload.size();
    ASSERT_EQ(written.size(), expectedWrittenBytes);
    const auto headerEnd =
        written.begin() + static_cast<std::ptrdiff_t>(adapter_socket::FrameHeaderDto::HEADER_BYTES);
    EXPECT_EQ(std::vector<uint8_t>(written.begin(), headerEnd), GetParam().expectedHeader);
}

TEST_P(FramedConnectionPayloadTest, RoundTripReturnsWrittenPayload)
{
    auto ownedWriterSide =
        std::make_unique<FakeSocketConnection>(std::vector<uint8_t>(), WHOLE_FRAME_CHUNK_BYTES);
    const FakeSocketConnection* const writerSide = ownedWriterSide.get();
    adapter_socket::FramedConnection writer(std::move(ownedWriterSide), STEADY_DEADLINES,
                                            FRAME_TIMEOUT);
    writer.writeFrame(GetParam().payload);

    auto ownedReaderSide =
        std::make_unique<FakeSocketConnection>(writerSide->written(), WHOLE_FRAME_CHUNK_BYTES);
    adapter_socket::FramedConnection reader(std::move(ownedReaderSide), STEADY_DEADLINES,
                                            FRAME_TIMEOUT);

    EXPECT_EQ(reader.readFrame(), GetParam().payload);
}

INSTANTIATE_TEST_SUITE_P(Payload, FramedConnectionPayloadTest, ::testing::ValuesIn(PAYLOADS),
                         caseName<PayloadCase>);
