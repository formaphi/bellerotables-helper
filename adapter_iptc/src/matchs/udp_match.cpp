#include "adapter_iptc/matchs/udp_match.h"

#include "adapter_iptc/rule_layout.h"

#include <linux/netfilter/xt_tcpudp.h>
#include <netinet/in.h>

UdpMatch::UdpMatch(uint16_t destinationPort)
    : m_destinationPorts{.min = destinationPort, .max = destinationPort}
{
}

UdpMatch::UdpMatch(PortRange destinationPorts)
    : m_destinationPorts(destinationPorts)
{
}

uint8_t UdpMatch::protocol() const
{
    return IPPROTO_UDP;
}

const char* UdpMatch::name() const
{
    return "udp";
}

uint8_t UdpMatch::revision() const
{
    return 0;
}

std::size_t UdpMatch::dataSize() const
{
    return sizeof(xt_udp);
}

void UdpMatch::fillData(unsigned char* data) const
{
    xt_udp info{};
    info.spts[0] = 0;
    info.spts[1] = 65535;
    info.dpts[0] = m_destinationPorts.min;
    info.dpts[1] = m_destinationPorts.max;
    RuleLayout::store(data, info);
}
