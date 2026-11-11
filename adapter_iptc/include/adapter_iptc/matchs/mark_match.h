#ifndef MARK_MATCH_H
#define MARK_MATCH_H

#include "adapter_iptc/template_match.h"

#include <cstdint>

class MarkMatch : public TemplateMatch
{
public:
    MarkMatch(uint32_t mark, uint32_t mask = 0xffffffff);

protected:
    const char* name() const override;
    uint8_t revision() const override;
    std::size_t dataSize() const override;
    void fillData(unsigned char* data) const override;

private:
    uint32_t m_mark;
    uint32_t m_mask;
};

#endif
