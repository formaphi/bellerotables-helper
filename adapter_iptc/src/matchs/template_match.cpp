#include "adapter_iptc/template_match.h"

#include <cstring>
#include <linux/netfilter/x_tables.h>

std::size_t TemplateMatch::size() const {
    return XT_ALIGN(sizeof(xt_entry_match) + dataSize());
}

void TemplateMatch::write(unsigned char *ptr) const {
    auto *m = reinterpret_cast<xt_entry_match *>(ptr);
    m->u.user.match_size = size();
    std::strncpy(m->u.user.name, name(), sizeof(m->u.user.name) - 1);
    m->u.user.revision = revision();
    fillData(m->data);
}

uint8_t TemplateMatch::protocol() const { return 0; }
