#include "adapter_iptc/adapters/mangle_adapter.h"
#include "adapter_iptc/builder/rule_builder.h"
#include "adapter_iptc/targets/standard_target.h"
#include "adapter_iptc/targets/nflog_target.h"
#include "adapter_iptc/matchs/mark_match.h"
#include "adapter_iptc/matchs/conntrack_match.h"

#include <memory>

MangleAdapter::MangleAdapter(const std::string& chain)
    : m_handle("mangle", chain)
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
    auto markMatch = std::make_shared<MarkMatch>(mark.mark);
    auto conntrackMatch = std::make_shared<ConntrackMatch>(conntrack.state);
    auto acceptTarget = std::make_shared<StandardTarget>(StandardTarget::Verdict::ACCEPT);

    auto entry = RuleBuilder()
        .setMatch(markMatch)
        .setMatch(conntrackMatch)
        .setTarget(acceptTarget)
        .build();

    m_handle.appendEntry(entry.get());
}

void MangleAdapter::addNflogRule(MarkParam mark, NflogParam nflog)
{
    auto markMatch = std::make_shared<MarkMatch>(mark.mark);
    auto nflogTarget = std::make_shared<NflogTarget>(nflog.group, nflog.prefix);

    auto entry = RuleBuilder()
        .setMatch(markMatch)
        .setTarget(nflogTarget)
        .build();

    m_handle.appendEntry(entry.get());
}

void MangleAdapter::commit()
{
    m_handle.commit();
}
