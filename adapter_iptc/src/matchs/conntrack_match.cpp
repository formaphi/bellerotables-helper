#include "adapter_iptc/matchs/conntrack_match.h"

#include <linux/netfilter/nf_conntrack_common.h>
#include <linux/netfilter/xt_conntrack.h>

ConntrackMatch::ConntrackMatch(State state) {
    resolve(state, m_matchFlags, m_stateMask);
}

void ConntrackMatch::resolve(State state, uint16_t &matchFlags, uint16_t &stateMask) {
    switch (state) {
    case State::NEW:
        matchFlags = XT_CONNTRACK_STATE | XT_CONNTRACK_DIRECTION;
        stateMask = NF_CT_STATE_BIT(IP_CT_NEW);
        break;
    case State::ESTABLISHED_RELATED:
        matchFlags = XT_CONNTRACK_STATE;
        stateMask = NF_CT_STATE_BIT(IP_CT_ESTABLISHED) | NF_CT_STATE_BIT(IP_CT_RELATED);
        break;
    case State::INVALID:
        matchFlags = XT_CONNTRACK_STATE;
        stateMask = NF_CT_STATE_INVALID_BIT;
        break;
    }
}

const char *ConntrackMatch::name() const { return "conntrack"; }
uint8_t ConntrackMatch::revision() const { return 3; }
std::size_t ConntrackMatch::dataSize() const { return sizeof(xt_conntrack_mtinfo3); }

void ConntrackMatch::fillData(unsigned char *data) const {
    auto *info = reinterpret_cast<xt_conntrack_mtinfo3 *>(data);
    info->match_flags = m_matchFlags;
    info->state_mask = m_stateMask;
}
