#ifndef MANGLE_ADAPTER_H
#define MANGLE_ADAPTER_H

#include "adapter_iptc/dtos/conntrack.h"
#include "adapter_iptc/dtos/mark.h"
#include "adapter_iptc/dtos/nflog.h"

#include <vector>

class IMangleAdapter
{
public:
    virtual ~IMangleAdapter() = default;

    virtual void createChain() = 0;
    virtual void flush() = 0;
    virtual void addConntrackRule(MarkParam mark, ConntrackParam conntrack) = 0;
    virtual void addNflogRule(MarkParam mark, NflogParam nflog) = 0;
    virtual void commit() = 0;

    /* LEARN: the NFLOG rules of the chain with the mark each one matches, in rule order, as the
       kernel holds them; other rules (conntrack, ...) are skipped. Empty when the chain does not
       exist. Called after commit(), it reads a fresh snapshot, so it shows what the kernel
       accepted, not what was added.
    */
    virtual std::vector<NflogRuleParam> readNflogRules() = 0;
};

#endif /* MANGLE_ADAPTER_H */
