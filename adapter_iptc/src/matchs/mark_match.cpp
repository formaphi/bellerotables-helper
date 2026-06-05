#include "adapter_iptc/matchs/mark_match.h"

#include <linux/netfilter/xt_mark.h>

MarkMatch::MarkMatch(uint32_t mark, uint32_t mask)
    : m_mark(mark), m_mask(mask) {}

const char *MarkMatch::name() const { return "mark"; }
uint8_t MarkMatch::revision() const { return 1; }
std::size_t MarkMatch::dataSize() const { return sizeof(xt_mark_mtinfo1); }

void MarkMatch::fillData(unsigned char *data) const {
    auto *info = reinterpret_cast<xt_mark_mtinfo1 *>(data);
    info->mark = m_mark;
    info->mask = m_mask;
}
