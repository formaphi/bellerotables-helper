#include "adapter_iptc/matchs/mark_match.h"

#include "adapter_iptc/dtos/mark.h"
#include "adapter_iptc/rule_layout.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_mark.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <optional>

namespace {

constexpr const char* MARK_MATCH_NAME = "mark";
constexpr uint8_t MARK_MATCH_REVISION = 1;
constexpr uint32_t EVERY_MARK_BIT = 0xffffffff;

} // namespace

MarkMatch::MarkMatch(uint32_t mark, uint32_t mask)
    : m_mark(mark)
    , m_mask(mask)
{
}

const char* MarkMatch::name() const
{
    return MARK_MATCH_NAME;
}

uint8_t MarkMatch::revision() const
{
    return MARK_MATCH_REVISION;
}

std::size_t MarkMatch::dataSize() const
{
    return sizeof(xt_mark_mtinfo1);
}

void MarkMatch::fillData(unsigned char* data) const
{
    xt_mark_mtinfo1 info{};
    info.mark = m_mark;
    info.mask = m_mask;
    RuleLayout::store(data, info);
}

std::optional<MarkParam> MarkMatch::read(const ipt_entry& entry)
{
    const unsigned char* data = dataOf(entry);
    if (data == nullptr)
    {
        return std::nullopt;
    }

    const auto info = RuleLayout::load<xt_mark_mtinfo1>(data);
    if (info.mask != EVERY_MARK_BIT || info.invert != 0)
    {
        return std::nullopt;
    }
    return MarkParam{.mark = info.mark};
}

/* LEARN: the matches of a rule sit one after another between the rule header and the target:

   sizeof(ipt_entry)                                                      target_offset
   | xt_entry_match | payload | padding | xt_entry_match | payload | padding | target ...
   |<------------- match_size -------->|<------------- match_size -------->|

   No index says where each match starts: every match records its own size (match_size), so the
   walk jumps match_size bytes to reach the next one. Those sizes come from the buffer itself and
   are checked before use: a match_size smaller than its header (0 would loop forever) or one
   running into the target stops the walk with nullptr instead of reading out of the rule.
*/
const unsigned char* MarkMatch::dataOf(const ipt_entry& entry)
{
    const std::size_t matchesStart = sizeof(ipt_entry);
    const std::size_t matchesEnd = entry.target_offset;
    const std::size_t entryEnd = entry.next_offset;

    // LEARN: the matches area must lie inside the rule: after its header, before its end.
    if (matchesEnd < matchesStart || matchesEnd > entryEnd)
    {
        return nullptr;
    }

    const unsigned char* entryStart = RuleLayout::bytesOf(entry);
    const std::size_t markMatchBytes = sizeof(xt_entry_match) + sizeof(xt_mark_mtinfo1);

    std::size_t matchOffset = matchesStart;
    while (true)
    {
        /* LEARN: no room for another match header before the target: every match was visited
           (or the rule has none), and none is the mark match.
        */
        const std::size_t matchHeaderEnd = matchOffset + sizeof(xt_entry_match);
        if (matchHeaderEnd > matchesEnd)
        {
            return nullptr;
        }

        const unsigned char* matchStart = entryStart + matchOffset;
        const ExtensionHeader match = RuleLayout::readMatchHeader(matchStart);

        // LEARN: match_size counts the match header plus its payload (and padding).
        const std::size_t matchBytes = match.size;
        const std::size_t matchEnd = matchOffset + matchBytes;
        if (matchBytes < sizeof(xt_entry_match) || matchEnd > matchesEnd)
        {
            return nullptr;
        }

        /* LEARN: the name field is fixed-size and may lack a terminator; strncmp never reads past
           it. Another revision of "mark" has another payload layout, so it is not decoded as
           xt_mark_mtinfo1, and a match too small for that payload is skipped as well.
        */
        const bool isMark = match.hasName(MARK_MATCH_NAME);
        const bool isWrittenRevision = match.revision == MARK_MATCH_REVISION;
        const bool holdsMarkInfo = matchBytes >= markMatchBytes;
        if (isMark && isWrittenRevision && holdsMarkInfo)
        {
            return RuleLayout::matchPayload(matchStart);
        }

        matchOffset = matchEnd;
    }
}
