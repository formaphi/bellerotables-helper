#ifndef I_IPTC_BUILDER_H
#define I_IPTC_BUILDER_H

#include "adapter_iptc/ipt_entry_ptr.h"

struct ipt_entry;

class IBuilder
{
public:
    virtual ~IBuilder() = default;
    virtual IptEntryPtr build() const = 0;
};

#endif
