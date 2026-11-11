#ifndef MARK_DTO_H
#define MARK_DTO_H

#include <cstdint>

/* LEARN: a plain "-m mark --mark <mark>" match: the whole mark compared, not inverted. */
struct MarkParam
{
    uint32_t mark;

    bool operator==(const MarkParam&) const = default;
};

#endif
