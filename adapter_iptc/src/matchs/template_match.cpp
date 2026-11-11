#include "adapter_iptc/template_match.h"

#include <cstring>
#include <linux/netfilter/x_tables.h>

std::size_t TemplateMatch::size() const
{
    return XT_ALIGN(sizeof(xt_entry_match) + dataSize());
}

void TemplateMatch::write(unsigned char* ptr) const
{
    auto* match = reinterpret_cast<xt_entry_match*>(ptr);
    match->u.user.match_size = size();
    std::strncpy(match->u.user.name, name(), sizeof(match->u.user.name) - 1);
    match->u.user.revision = revision();
    fillData(match->data);
}

uint8_t TemplateMatch::protocol() const
{
    return 0;
}
