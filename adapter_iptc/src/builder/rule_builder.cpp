#include "adapter_iptc/builder/rule_builder.h"

#include "adapter_iptc/ipt_entry_ptr.h"

#include <cstdlib>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <new>

RuleBuilder& RuleBuilder::setMatch(const std::shared_ptr<TemplateMatch>& m)
{
    matches_.push_back(m);
    return *this;
}

RuleBuilder& RuleBuilder::setTarget(const std::shared_ptr<ITargetStrategy>& t)
{
    target_ = t;
    return *this;
}

IptEntryPtr RuleBuilder::build() const
{
    std::size_t matchesSize = 0;
    for (const auto& m : matches_)
        matchesSize += m->size();

    auto targetOffset = sizeof(ipt_entry) + matchesSize;
    auto total = targetOffset + target_->size();

    void* buffer = std::calloc(1, total);
    if (buffer == nullptr)
    {
        throw std::bad_alloc();
    }
    auto* rule = static_cast<ipt_entry*>(buffer);
    IptEntryPtr entry(rule, &std::free);
    entry->target_offset = targetOffset;
    entry->next_offset = total;
    entry->ip.proto = resolveProtocol();

    auto* ptr = entry->elems;
    for (const auto& m : matches_)
    {
        m->write(ptr);
        ptr += m->size();
    }

    target_->write(ptr);
    return entry;
}

uint8_t RuleBuilder::resolveProtocol() const
{
    for (const auto& m : matches_)
    {
        auto proto = m->protocol();
        if (proto != 0)
            return proto;
    }
    return 0;
}
