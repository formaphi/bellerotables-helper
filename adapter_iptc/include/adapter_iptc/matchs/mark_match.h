#ifndef MARK_MATCH_H
#define MARK_MATCH_H

#include "adapter_iptc/dtos/mark.h"
#include "adapter_iptc/template_match.h"

#include <cstdint>
#include <optional>

struct ipt_entry;

class MarkMatch : public TemplateMatch
{
public:
    explicit MarkMatch(uint32_t mark);
    explicit MarkMatch(MaskedMark maskedMark);

    /* LEARN: decodes the entry's first mark match read back from the kernel. nullopt when there
       is none, or when it is not a plain match (mask other than every bit, or inverted): MarkParam
       cannot express those, and reporting only the mark would describe a different rule.
    */
    [[nodiscard]] static std::optional<MarkParam> read(const ipt_entry& entry);

protected:
    [[nodiscard]] const char* name() const override;
    [[nodiscard]] uint8_t revision() const override;
    [[nodiscard]] std::size_t dataSize() const override;
    void fillData(unsigned char* data) const override;

private:
    /* LEARN: finds the payload (xt_mark_mtinfo1) of the first "mark" match inside one rule
       buffer, at the revision this class writes. Returns nullptr when the rule has no such
       match, or when the sizes stored in the buffer do not fit inside it (see the layout drawn
       at the definition).
    */
    static const unsigned char* dataOf(const ipt_entry& entry);

    uint32_t m_mark;
    uint32_t m_mask;
};

#endif
