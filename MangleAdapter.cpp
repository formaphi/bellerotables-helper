#include "MangleAdapter.h"
#include "iptc_builder.h"
#include "iptc_target.h"

#include <cstdlib>
#include <linux/netfilter/nf_conntrack_common.h>
#include <memory>

struct AdapterFreeDeleter { void operator()(void* p) { std::free(p); } };
using AdapterEntryPtr = std::unique_ptr<ipt_entry, AdapterFreeDeleter>;

static uint16_t toStateMask(const std::vector<ConntrackState>& states) {
    uint16_t mask = 0;
    for (auto state : states) {
        switch (state) {
            case ConntrackState::NEW:         mask |= NF_CT_STATE_BIT(IP_CT_NEW);         break;
            case ConntrackState::ESTABLISHED: mask |= NF_CT_STATE_BIT(IP_CT_ESTABLISHED); break;
            case ConntrackState::RELATED:     mask |= NF_CT_STATE_BIT(IP_CT_RELATED);     break;
            case ConntrackState::INVALID:     mask |= NF_CT_STATE_INVALID_BIT;            break;
        }
    }
    return mask;
}

MangleAdapter::MangleAdapter(ILibiptcHandle& handle, const std::string& chain)
    : m_handle(handle)
    , m_chain(chain)
{
}

void MangleAdapter::createChain()
{
    m_handle.createChain();
}

void MangleAdapter::flush()
{
    m_handle.flushEntries();
}

void MangleAdapter::addConntrackRule(MarkParam mark, ConntrackParam conntrack)
{
    MarkMatch markMatch(mark.mark);
    ConntrackMatch conntrackMatch(XT_CONNTRACK_STATE, toStateMask(conntrack.states));
    StandardTarget acceptTarget(StandardTarget::Verdict::ACCEPT);

    AdapterEntryPtr entry(RuleBuilder()
        .addMatch(&markMatch)
        .addMatch(&conntrackMatch)
        .setTarget(&acceptTarget)
        .build());

    m_handle.appendEntry(entry.get());
}

void MangleAdapter::addNflogRule(MarkParam mark, NflogParam nflog)
{
    MarkMatch markMatch(mark.mark);
    NflogTarget nflogTarget(nflog.group, nflog.prefix.c_str());

    AdapterEntryPtr entry(RuleBuilder()
        .addMatch(&markMatch)
        .setTarget(&nflogTarget)
        .build());

    m_handle.appendEntry(entry.get());
}

void MangleAdapter::commit()
{
    m_handle.commit();
}
