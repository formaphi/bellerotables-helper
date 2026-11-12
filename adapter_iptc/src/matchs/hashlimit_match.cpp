#include "adapter_iptc/matchs/hashlimit_match.h"

#include "adapter_iptc/rule_layout.h"

#include <cstring>
#include <linux/netfilter/xt_hashlimit.h>
#include <utility>

HashlimitMatch::HashlimitMatch(HashlimitParam hashlimit)
    : m_hashlimit(std::move(hashlimit))
{
    if (m_hashlimit.mode == 0U)
    {
        m_hashlimit.mode = XT_HASHLIMIT_HASH_SIP | XT_HASHLIMIT_HASH_DIP | XT_HASHLIMIT_HASH_DPT;
    }
}

const char* HashlimitMatch::name() const
{
    return "hashlimit";
}

uint8_t HashlimitMatch::revision() const
{
    return 1;
}

std::size_t HashlimitMatch::dataSize() const
{
    return sizeof(xt_hashlimit_mtinfo1);
}

void HashlimitMatch::fillData(unsigned char* data) const
{
    xt_hashlimit_mtinfo1 info{};
    std::strncpy(info.name, m_hashlimit.name.c_str(), sizeof(info.name) - 1);
    info.cfg.mode = m_hashlimit.mode;
    info.cfg.avg = m_hashlimit.average;
    info.cfg.burst = m_hashlimit.burst;
    info.cfg.expire = m_hashlimit.expire;
    info.cfg.gc_interval = m_hashlimit.garbageCollectionInterval;
    info.cfg.srcmask = m_hashlimit.sourceMask;
    info.cfg.dstmask = m_hashlimit.destinationMask;
    RuleLayout::store(data, info);
}
