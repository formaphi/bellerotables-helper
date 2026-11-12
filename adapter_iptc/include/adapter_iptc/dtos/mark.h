#ifndef MARK_DTO_H
#define MARK_DTO_H

#include <cstdint>

/* LEARN: a plain "-m mark --mark <mark>" match: the whole mark compared, not inverted. */
struct MarkParam
{
    uint32_t mark;

    bool operator==(const MarkParam&) const = default;
};

/* LEARN: a mark match with an explicit mask, "-m mark --mark mark/mask". Not MarkParam: that one is
   the plain match the rest of the library reads back and reports.
*/
struct MaskedMark
{
    uint32_t mark;
    uint32_t mask;

    bool operator==(const MaskedMark&) const = default;
};

#endif
