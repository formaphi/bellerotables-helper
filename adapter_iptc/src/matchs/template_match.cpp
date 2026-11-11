#include "adapter_iptc/template_match.h"

#include "adapter_iptc/rule_layout.h"

#include <cstring>
#include <linux/netfilter/x_tables.h>

std::size_t TemplateMatch::size() const
{
    return XT_ALIGN(sizeof(xt_entry_match) + dataSize());
}

void TemplateMatch::write(unsigned char* ptr) const
{
    RuleLayout::writeMatchHeader(ptr, size(), name(), revision());
    fillData(RuleLayout::matchPayload(ptr));
}

uint8_t TemplateMatch::protocol() const
{
    return 0;
}
