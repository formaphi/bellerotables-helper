#include "adapter_iptc/rule_layout.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <new>

bool ExtensionHeader::hasName(const char* expected) const
{
    return std::strncmp(name.data(), expected, name.size()) == 0;
}

IptEntryPtr RuleLayout::allocateEntry(std::size_t bytes)
{
    // LEARN: rule buffer.
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
    void* buffer = std::calloc(1, bytes);
    if (buffer == nullptr)
    {
        throw std::bad_alloc();
    }
    return {static_cast<ipt_entry*>(buffer), &std::free};
}

// LEARN: the only union access. NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)

void RuleLayout::writeMatchHeader(unsigned char* position, std::size_t size, const char* name,
                                  uint8_t revision)
{
    xt_entry_match header{};
    header.u.user.match_size = size;
    std::strncpy(header.u.user.name, name, sizeof(header.u.user.name) - 1);
    header.u.user.revision = revision;
    store(position, header);
}

void RuleLayout::writeTargetHeader(unsigned char* position, std::size_t size, const char* name)
{
    xt_entry_target header{};
    header.u.user.target_size = size;
    std::strncpy(header.u.user.name, name, sizeof(header.u.user.name) - 1);
    store(position, header);
}

std::size_t RuleLayout::standardTargetSize()
{
    return XT_ALIGN(sizeof(xt_standard_target));
}

void RuleLayout::writeStandardTarget(unsigned char* position, int verdict)
{
    xt_standard_target standard{};
    standard.target.u.user.target_size = standardTargetSize();
    standard.verdict = verdict;
    store(position, standard);
}

ExtensionHeader RuleLayout::readMatchHeader(const unsigned char* position)
{
    const auto header = load<xt_entry_match>(position);
    ExtensionHeader result{
        .size = header.u.user.match_size, .revision = header.u.user.revision, .name = {}};
    std::copy_n(header.u.user.name, result.name.size(), result.name.begin());
    return result;
}

ExtensionHeader RuleLayout::readTargetHeader(const unsigned char* position)
{
    const auto header = load<xt_entry_target>(position);
    ExtensionHeader result{.size = header.u.user.target_size, .revision = 0, .name = {}};
    std::copy_n(header.u.user.name, result.name.size(), result.name.begin());
    return result;
}

// LEARN: end of the union access. NOLINTEND(cppcoreguidelines-pro-type-union-access)

const unsigned char* RuleLayout::matchPayload(const unsigned char* position)
{
    return position + offsetof(xt_entry_match, data);
}

const unsigned char* RuleLayout::targetPayload(const unsigned char* position)
{
    return position + offsetof(xt_entry_target, data);
}

unsigned char* RuleLayout::matchPayload(unsigned char* position)
{
    return position + offsetof(xt_entry_match, data);
}

unsigned char* RuleLayout::targetPayload(unsigned char* position)
{
    return position + offsetof(xt_entry_target, data);
}

const unsigned char* RuleLayout::bytesOf(const ipt_entry& entry)
{
    // LEARN: an object viewed as bytes. NOLINTNEXTLINE(bugprone-casting-through-void)
    return static_cast<const unsigned char*>(static_cast<const void*>(&entry));
}
