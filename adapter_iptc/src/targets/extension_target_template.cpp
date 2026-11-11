#include "adapter_iptc/targets/extension_target_template.h"

#include "adapter_iptc/rule_layout.h"

#include <cstring>
#include <linux/netfilter/x_tables.h>

std::size_t ExtensionTargetTemplate::size() const
{
    return XT_ALIGN(sizeof(xt_entry_target) + dataSize());
}

void ExtensionTargetTemplate::write(unsigned char* ptr) const
{
    RuleLayout::writeTargetHeader(ptr, size(), name());
    fillData(RuleLayout::targetPayload(ptr));
}
