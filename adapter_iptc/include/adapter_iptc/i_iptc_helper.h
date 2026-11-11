#ifndef I_IPTC_HELPER_H
#define I_IPTC_HELPER_H

#include "adapter_iptc/ipt_entry_ptr.h"

#include <vector>

struct ipt_entry;

class IIptcHelper
{
public:
    virtual ~IIptcHelper() = default;
    virtual void createChain() = 0;
    virtual void flushEntries() = 0;
    virtual void appendEntry(const ipt_entry* entry) = 0;
    virtual void commit() = 0;

    /* LEARN: the read side. Both look at the handle's snapshot: after commit() the next call takes
       a fresh snapshot, so what they return is what the kernel holds, not what was appended.
       listEntries() returns copies in rule order, so they outlive the handle; it throws when the
       chain does not exist, which chainExists() lets the caller check first.
    */
    virtual bool chainExists() = 0;
    virtual std::vector<IptEntryPtr> listEntries() = 0;
};

#endif
