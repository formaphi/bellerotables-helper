#include "adapter_iptc/targets/extension_target_template.h"

#include <cstring>
#include <linux/netfilter/x_tables.h>

std::size_t ExtensionTargetTemplate::size() const {
    return XT_ALIGN(sizeof(xt_entry_target) + dataSize());
}

void ExtensionTargetTemplate::write(unsigned char *ptr) const {
    auto *t = reinterpret_cast<xt_entry_target *>(ptr);
    t->u.user.target_size = size();
    std::strncpy(t->u.user.name, name(), sizeof(t->u.user.name) - 1);
    fillData(t->data);
}
