#include "adapter_iptc/targets/nflog_target.h"

#include <cstring>
#include <linux/netfilter/xt_NFLOG.h>

NflogTarget::NflogTarget(uint16_t group, const std::string& prefix)
    : m_group(group)
    , m_prefix(prefix)
{
}

const char *NflogTarget::name() const { return "NFLOG"; }

std::size_t NflogTarget::dataSize() const { return sizeof(xt_nflog_info); }

void NflogTarget::fillData(unsigned char *data) const
{
    auto *info = reinterpret_cast<xt_nflog_info *>(data);
    info->group = m_group;
    std::strncpy(info->prefix, m_prefix.c_str(), sizeof(info->prefix) - 1);
}
