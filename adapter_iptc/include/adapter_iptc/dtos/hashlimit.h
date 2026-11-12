#ifndef HASHLIMIT_DTO_H
#define HASHLIMIT_DTO_H

#include <cstdint>
#include <string>

/* LEARN: the knobs of "-m hashlimit" in kernel widths (xt_hashlimit_cfg1). Aggregate with named
   fields, built with designated initializers, so the seven values cannot be passed in the wrong
   order. mode 0 means "use the default": hash on source address, destination address and port.
*/
struct HashlimitParam
{
    std::string name;
    uint32_t average;
    uint32_t burst;
    uint32_t expire;
    uint32_t garbageCollectionInterval = 1000;
    uint8_t sourceMask = 32;
    uint8_t destinationMask = 32;
    uint32_t mode = 0;
};

#endif
