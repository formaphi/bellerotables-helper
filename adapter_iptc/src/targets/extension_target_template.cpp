#include "adapter_iptc/targets/extension_target_template.h"

#include <cstring>
#include <linux/netfilter/x_tables.h>

std::size_t ExtensionTargetTemplate::size() const
{
    return XT_ALIGN(sizeof(xt_entry_target) + dataSize());
}

void ExtensionTargetTemplate::write(unsigned char* ptr) const
{
    auto* target = reinterpret_cast<xt_entry_target*>(ptr);
    target->u.user.target_size = size();
    std::strncpy(target->u.user.name, name(), sizeof(target->u.user.name) - 1);
    fillData(target->data);
}
