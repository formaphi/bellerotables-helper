#ifndef TCP_MATCH_H
#define TCP_MATCH_H

#include "adapter_iptc/dtos/port_range.h"
#include "adapter_iptc/template_match.h"

#include <cstdint>

class TcpMatch : public TemplateMatch
{
public:
    explicit TcpMatch(uint16_t destinationPort);
    explicit TcpMatch(PortRange destinationPorts);

    [[nodiscard]] uint8_t protocol() const override;

protected:
    [[nodiscard]] const char* name() const override;
    [[nodiscard]] uint8_t revision() const override;
    [[nodiscard]] std::size_t dataSize() const override;
    void fillData(unsigned char* data) const override;

private:
    PortRange m_destinationPorts;
};

#endif
