#include "adapter_iptc/matchs/tcp_match.h"

#include <netinet/in.h>
#include <linux/netfilter/xt_tcpudp.h>

TcpMatch::TcpMatch(uint16_t destinationPort)
    : m_destinationPortMin(destinationPort), m_destinationPortMax(destinationPort) {}

TcpMatch::TcpMatch(uint16_t destinationPortMin, uint16_t destinationPortMax)
    : m_destinationPortMin(destinationPortMin), m_destinationPortMax(destinationPortMax) {}

uint8_t TcpMatch::protocol() const { return IPPROTO_TCP; }

const char *TcpMatch::name() const { return "tcp"; }
uint8_t TcpMatch::revision() const { return 0; }
std::size_t TcpMatch::dataSize() const { return sizeof(xt_tcp); }

void TcpMatch::fillData(unsigned char *data) const {
    auto *info = reinterpret_cast<xt_tcp *>(data);
    info->spts[0] = 0;
    info->spts[1] = 65535;
    info->dpts[0] = m_destinationPortMin;
    info->dpts[1] = m_destinationPortMax;
}
