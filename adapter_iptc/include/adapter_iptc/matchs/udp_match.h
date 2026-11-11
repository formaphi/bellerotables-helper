#ifndef UDP_MATCH_H
#define UDP_MATCH_H

#include "adapter_iptc/template_match.h"

#include <cstdint>

class UdpMatch : public TemplateMatch
{
public:
    explicit UdpMatch(uint16_t destinationPort);
    UdpMatch(uint16_t destinationPortMin, uint16_t destinationPortMax);

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
