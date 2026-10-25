#ifndef ADAPTER_SOCKET_FRAME_HEADER_DTO_H
#define ADAPTER_SOCKET_FRAME_HEADER_DTO_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace adapter_socket {

struct FrameHeaderDto
{
    static constexpr std::size_t HEADER_BYTES = 4;
    static constexpr std::size_t MAX_PAYLOAD_BYTES = 65536;
    static constexpr uint32_t EMPTY_PAYLOAD_BYTES = 0;

    uint32_t payloadLength = EMPTY_PAYLOAD_BYTES;

    [[nodiscard]] static FrameHeaderDto parse(const std::vector<uint8_t>& headerBytes);
    [[nodiscard]] std::vector<uint8_t> toBytes() const;
};

} // namespace adapter_socket

#endif // ADAPTER_SOCKET_FRAME_HEADER_DTO_H
