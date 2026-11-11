#ifndef TCP_MATCH_H
#define TCP_MATCH_H

#include "adapter_iptc/template_match.h"

#include <cstdint>

class TcpMatch : public TemplateMatch
{
public:
    explicit TcpMatch(uint16_t destinationPort);
    TcpMatch(uint16_t destinationPortMin, uint16_t destinationPortMax);

    uint8_t protocol() const override;

protected:
    const char* name() const override;
    uint8_t revision() const override;
    std::size_t dataSize() const override;
    void fillData(unsigned char* data) const override;

private:
    uint16_t m_destinationPortMin;
    uint16_t m_destinationPortMax;
};

#endif
