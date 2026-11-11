#include "adapter_iptc/adapters/mangle_adapter.h"

#include "adapter_iptc/builder/rule_builder.h"
#include "adapter_iptc/matchs/conntrack_match.h"
#include "adapter_iptc/matchs/mark_match.h"
#include "adapter_iptc/targets/nflog_target.h"
#include "adapter_iptc/targets/standard_target.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

MangleAdapter::MangleAdapter(const std::string& chain)
    : m_helper(std::make_shared<IptcHelper>("mangle", chain))
{
}

/* LEARN: every activation re-applies the desired state, so the chain may already exist;
   creating it only when missing makes createChain() safe to repeat.
*/
void MangleAdapter::createChain()
{
    if (!m_helper->chainExists())
    {
        m_helper->createChain();
    }
}

void MangleAdapter::flush()
{
    m_helper->flushEntries();
}

void MangleAdapter::addConntrackRule(MarkParam mark, ConntrackParam conntrack)
{
    auto markMatch = std::make_shared<MarkMatch>(mark.mark);
    auto conntrackMatch = std::make_shared<ConntrackMatch>(conntrack.state);
    auto acceptTarget = std::make_shared<StandardTarget>(StandardTarget::Verdict::ACCEPT);

    auto entry =
        RuleBuilder().setMatch(markMatch).setMatch(conntrackMatch).setTarget(acceptTarget).build();

    m_helper->appendEntry(entry.get());
}

void MangleAdapter::addNflogRule(MarkParam mark, NflogParam nflog)
{
    auto markMatch = std::make_shared<MarkMatch>(mark.mark);
    auto nflogTarget = std::make_shared<NflogTarget>(std::move(nflog));

    auto entry = RuleBuilder().setMatch(markMatch).setTarget(nflogTarget).build();

    m_helper->appendEntry(entry.get());
}

void MangleAdapter::setHelper(std::shared_ptr<IIptcHelper> helper)
{
    m_helper = helper;
}

void MangleAdapter::commit()
{
    m_helper->commit();
}

std::vector<NflogRuleParam> MangleAdapter::readNflogRules()
{
    std::vector<NflogRuleParam> rules;
    if (!m_helper->chainExists())
    {
        return rules;
    }
    for (const IptEntryPtr& entry : m_helper->listEntries())
    {
        std::optional<NflogParam> nflog = NflogTarget::read(*entry);
        if (nflog.has_value())
        {
            rules.push_back(
                NflogRuleParam{.mark = MarkMatch::read(*entry), .nflog = std::move(*nflog)});
        }
    }
    return rules;
}
