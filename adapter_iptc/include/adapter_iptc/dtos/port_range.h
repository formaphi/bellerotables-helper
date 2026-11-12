#ifndef PORT_RANGE_DTO_H
#define PORT_RANGE_DTO_H

#include <cstdint>

/* LEARN: an inclusive destination port range, "--dport min:max". A single port is a range whose
   two ends are equal. Named fields keep min and max from being swapped at the call site.
*/
struct PortRange
{
    uint16_t min;
    uint16_t max;

    bool operator==(const PortRange&) const = default;
};

#endif
