#include "adapter_iptc/matchs/hashlimit_match.h"

#include <cstring>
#include <linux/netfilter/xt_hashlimit.h>

HashlimitMatch::HashlimitMatch(const std::string& hashlimitName, uint32_t average, uint32_t burst,
                               uint32_t expire, uint32_t garbageCollectionInterval,
                               uint8_t sourceMask, uint8_t destinationMask, uint32_t mode)
    : m_hashlimitName(hashlimitName)
    , m_average(average)
    , m_burst(burst)
    , m_expire(expire)
    , m_garbageCollectionInterval(garbageCollectionInterval)
    , m_sourceMask(sourceMask)
    , m_destinationMask(destinationMask)
    , m_mode(mode ? mode : XT_HASHLIMIT_HASH_SIP | XT_HASHLIMIT_HASH_DIP | XT_HASHLIMIT_HASH_DPT)
{
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
    auto* info = reinterpret_cast<xt_hashlimit_mtinfo1*>(data);
    std::strncpy(info->name, m_hashlimitName.c_str(), sizeof(info->name) - 1);
    info->cfg.mode = m_mode;
    info->cfg.avg = m_average;
    info->cfg.burst = m_burst;
    info->cfg.expire = m_expire;
    info->cfg.gc_interval = m_garbageCollectionInterval;
    info->cfg.srcmask = m_sourceMask;
    info->cfg.dstmask = m_destinationMask;
}
