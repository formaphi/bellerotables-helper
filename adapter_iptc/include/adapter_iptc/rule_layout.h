#ifndef RULE_LAYOUT_H
#define RULE_LAYOUT_H

#include "adapter_iptc/ipt_entry_ptr.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <linux/netfilter/x_tables.h>
#include <type_traits>

struct ipt_entry;

/* LEARN: what a match or target header says about the extension that follows it: the sizes and
   the name the kernel stored. The name field is fixed-size and may lack a terminator, which is why
   it is compared with hasName() and never read as a C string.
*/
struct ExtensionHeader
{
    std::size_t size;
    uint8_t revision;
    std::array<char, XT_EXTENSION_MAXNAMELEN> name;

    [[nodiscard]] bool hasName(const char* expected) const;
};

/* LEARN: the one place that knows the byte layout of an ipt_entry. A rule is a single buffer
   (header, matches, target) that the kernel defines, so reading and writing it means turning bytes
   into the xt_* structs. Everything that would trip clang-tidy lives here and nowhere else:
   - the union u.user / u.kernel of the match and target headers is touched only in rule_layout.cpp;
   - the rule buffer comes from calloc, because IptEntryPtr owns it with std::free;
   - the payload structs (xt_nflog_info, xt_mark_mtinfo1, ...) are copied with memcpy to and from a
     local value, so no reinterpret_cast is needed: store() and load().
   All members are static and stateless.
*/
class RuleLayout
{
public:
    RuleLayout() = delete;

    // Allocates a zeroed rule buffer of `bytes`, owned by the returned pointer.
    [[nodiscard]] static IptEntryPtr allocateEntry(std::size_t bytes);

    // Writes a match header at `position`; `position` must have room for sizeof(xt_entry_match).
    static void writeMatchHeader(unsigned char* position, std::size_t size, const char* name,
                                 uint8_t revision);

    // Writes a target header at `position`; `position` must have room for sizeof(xt_entry_target).
    static void writeTargetHeader(unsigned char* position, std::size_t size, const char* name);

    // Size of a standard target (header and verdict) with the padding the kernel requires.
    [[nodiscard]] static std::size_t standardTargetSize();

    // Writes a whole standard target (header and verdict) at `position`.
    static void writeStandardTarget(unsigned char* position, int verdict);

    // The caller checks that sizeof(xt_entry_match) / sizeof(xt_entry_target) bytes are readable.
    [[nodiscard]] static ExtensionHeader readMatchHeader(const unsigned char* position);
    [[nodiscard]] static ExtensionHeader readTargetHeader(const unsigned char* position);

    // Where the payload of the match / target that starts at `position` begins.
    [[nodiscard]] static const unsigned char* matchPayload(const unsigned char* position);
    [[nodiscard]] static const unsigned char* targetPayload(const unsigned char* position);
    [[nodiscard]] static unsigned char* matchPayload(unsigned char* position);
    [[nodiscard]] static unsigned char* targetPayload(unsigned char* position);

    // The first byte of a rule, to walk it by offsets.
    [[nodiscard]] static const unsigned char* bytesOf(const ipt_entry& entry);

    template <typename Payload>
    static void store(unsigned char* position, const Payload& value)
    {
        static_assert(std::is_trivially_copyable_v<Payload>);
        std::memcpy(position, &value, sizeof(Payload));
    }

    template <typename Payload>
    [[nodiscard]] static Payload load(const unsigned char* position)
    {
        static_assert(std::is_trivially_copyable_v<Payload>);
        Payload value{};
        std::memcpy(&value, position, sizeof(Payload));
        return value;
    }
};

#endif
