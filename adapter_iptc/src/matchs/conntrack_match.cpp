#include "adapter_iptc/matchs/conntrack_match.h"

#include "adapter_iptc/rule_layout.h"

#include <linux/netfilter/nf_conntrack_common.h>
#include <linux/netfilter/xt_conntrack.h>

ConntrackMatch::ConntrackMatch(State state)
    : m_flags(resolve(state))
{
}

ConntrackMatch::Flags ConntrackMatch::resolve(State state)
{
    switch (state)
    {
        case State::NEW:
            return {.matchFlags = XT_CONNTRACK_STATE | XT_CONNTRACK_DIRECTION,
                    .stateMask = NF_CT_STATE_BIT(IP_CT_NEW)};
        case State::ESTABLISHED_RELATED:
            return {
                .matchFlags = XT_CONNTRACK_STATE,
                .stateMask = NF_CT_STATE_BIT(IP_CT_ESTABLISHED) | NF_CT_STATE_BIT(IP_CT_RELATED)};
        case State::INVALID:
            return {.matchFlags = XT_CONNTRACK_STATE, .stateMask = NF_CT_STATE_INVALID_BIT};
    }
    return {.matchFlags = XT_CONNTRACK_STATE, .stateMask = NF_CT_STATE_INVALID_BIT};
}

const char* ConntrackMatch::name() const
{
    return "conntrack";
}

uint8_t ConntrackMatch::revision() const
{
    return 3;
}

std::size_t ConntrackMatch::dataSize() const
{
    return sizeof(xt_conntrack_mtinfo3);
}

void ConntrackMatch::fillData(unsigned char* data) const
{
    xt_conntrack_mtinfo3 info{};
    info.match_flags = m_flags.matchFlags;
    info.state_mask = m_flags.stateMask;
    RuleLayout::store(data, info);
}
