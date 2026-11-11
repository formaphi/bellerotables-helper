#include "adapter_iptc/targets/nflog_target.h"

#include "adapter_iptc/dtos/nflog.h"

#include <cstddef>
#include <cstring>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_NFLOG.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <optional>
#include <string>
#include <utility>

namespace {

constexpr const char* NFLOG_TARGET_NAME = "NFLOG";
constexpr unsigned char UTF8_CONTINUATION_MASK = 0xC0;
constexpr unsigned char UTF8_CONTINUATION_BITS = 0x80;

bool isUtf8Continuation(char byte)
{
    return (static_cast<unsigned char>(byte) & UTF8_CONTINUATION_MASK) == UTF8_CONTINUATION_BITS;
}

/* LEARN: the contract bounds the prefix in characters, the kernel in bytes (63 + NUL). A plain
   byte cut can split a multi-byte character and leave invalid UTF-8 in the kernel, so the cut
   moves back to the start of the character it would split.
*/
std::size_t prefixBytesThatFit(const std::string& prefix, std::size_t maxBytes)
{
    if (prefix.size() <= maxBytes)
    {
        return prefix.size();
    }
    std::size_t cut = maxBytes;
    while (cut > 0 && isUtf8Continuation(prefix[cut]))
    {
        --cut;
    }
    return cut;
}

} // namespace

NflogTarget::NflogTarget(uint16_t group, const std::string& prefix)
    : NflogTarget(NflogParam{.prefix = prefix, .group = group})
{
}

NflogTarget::NflogTarget(NflogParam nflog)
    : m_nflog(std::move(nflog))
{
}

const char* NflogTarget::name() const
{
    return NFLOG_TARGET_NAME;
}

std::size_t NflogTarget::dataSize() const
{
    return sizeof(xt_nflog_info);
}

void NflogTarget::fillData(unsigned char* data) const
{
    auto* info = reinterpret_cast<xt_nflog_info*>(data);
    info->group = m_nflog.group;

    // LEARN: one byte of the field is kept for the terminator.
    const std::size_t prefixCapacity = sizeof(info->prefix) - 1;
    const std::size_t prefixBytes = prefixBytesThatFit(m_nflog.prefix, prefixCapacity);
    std::memset(info->prefix, 0, sizeof(info->prefix));
    std::memcpy(info->prefix, m_nflog.prefix.data(), prefixBytes);

    if (m_nflog.range.has_value())
    {
        info->len = *m_nflog.range;
        info->flags |= XT_NFLOG_F_COPY_LEN;
    }
    if (m_nflog.threshold.has_value())
    {
        info->threshold = *m_nflog.threshold;
    }
}

std::optional<NflogParam> NflogTarget::read(const ipt_entry& entry)
{
    const unsigned char* data = dataOf(entry);
    if (data == nullptr)
    {
        return std::nullopt;
    }

    const auto* info = reinterpret_cast<const xt_nflog_info*>(data);
    // LEARN: bounded by the field: a prefix filling all 64 bytes has no terminator.
    const std::size_t prefixBytes = strnlen(info->prefix, sizeof(info->prefix));
    NflogParam nflog{.prefix = std::string(info->prefix, prefixBytes), .group = info->group};
    if ((info->flags & XT_NFLOG_F_COPY_LEN) != 0)
    {
        nflog.range = info->len;
    }
    if (info->threshold != XT_NFLOG_DEFAULT_THRESHOLD)
    {
        nflog.threshold = info->threshold;
    }
    return nflog;
}

/* LEARN: one rule (ipt_entry) is a single buffer, laid out as RuleBuilder writes it and as the
   kernel hands it back:

   0            sizeof(ipt_entry)     target_offset                                   next_offset
   | ipt_entry  | matches (0..n)      | xt_entry_target | xt_nflog_info | padding     |
                                      |<------------------ target_size ------------->|

   Every offset and size used below is read from the buffer itself, so none is trusted: each one
   is checked against the others before it is used to read memory. A value pointing outside the
   rule returns nullptr instead of reading past the end of the buffer.
*/
const unsigned char* NflogTarget::dataOf(const ipt_entry& entry)
{
    const std::size_t targetOffset = entry.target_offset;
    const std::size_t entryEnd = entry.next_offset;

    // LEARN: the target starts after the rule header, and its own header must fit in the rule.
    const std::size_t targetHeaderEnd = targetOffset + sizeof(xt_entry_target);
    if (targetOffset < sizeof(ipt_entry) || targetHeaderEnd > entryEnd)
    {
        return nullptr;
    }

    const auto* entryStart = reinterpret_cast<const unsigned char*>(&entry);
    const unsigned char* targetStart = entryStart + targetOffset;
    const auto* target = reinterpret_cast<const xt_entry_target*>(targetStart);

    /* LEARN: target_size counts the target header plus its payload (and padding). It must be
       large enough for a whole xt_nflog_info, and the target must end inside the rule.
    */
    const std::size_t targetBytes = target->u.user.target_size;
    const std::size_t nflogTargetBytes = sizeof(xt_entry_target) + sizeof(xt_nflog_info);
    const std::size_t targetEnd = targetOffset + targetBytes;
    if (targetBytes < nflogTargetBytes || targetEnd > entryEnd)
    {
        return nullptr;
    }

    /* LEARN: the name field is fixed-size and may lack a terminator; strncmp never reads past it.
       Any other name (another target, or the empty name of ACCEPT/DROP) is not NFLOG.
    */
    const std::size_t nameFieldBytes = sizeof(target->u.user.name);
    if (std::strncmp(target->u.user.name, NFLOG_TARGET_NAME, nameFieldBytes) != 0)
    {
        return nullptr;
    }
    return target->data;
}
