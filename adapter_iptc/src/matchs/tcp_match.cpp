#include "adapter_iptc/matchs/tcp_match.h"

#include "adapter_iptc/rule_layout.h"

#include <linux/netfilter/xt_tcpudp.h>
#include <netinet/in.h>

TcpMatch::TcpMatch(uint16_t destinationPort)
    : m_destinationPorts{.min = destinationPort, .max = destinationPort}
{
}

TcpMatch::TcpMatch(PortRange destinationPorts)
    : m_destinationPorts(destinationPorts)
{
}

uint8_t TcpMatch::protocol() const
{
    return IPPROTO_TCP;
}

const char* TcpMatch::name() const
{
    return "tcp";
}

uint8_t TcpMatch::revision() const
{
    return 0;
}

std::size_t TcpMatch::dataSize() const
{
    return sizeof(xt_tcp);
}

void TcpMatch::fillData(unsigned char* data) const
{
    xt_tcp info{};
    info.spts[0] = 0;
    info.spts[1] = 65535;
    info.dpts[0] = m_destinationPorts.min;
    info.dpts[1] = m_destinationPorts.max;
    RuleLayout::store(data, info);
}
