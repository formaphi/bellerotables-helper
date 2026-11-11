#include "adapter_iptc/targets/standard_target.h"

#include "adapter_iptc/rule_layout.h"

#include <linux/netfilter.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter_ipv4/ip_tables.h>

StandardTarget::StandardTarget(Verdict verdict)
    : verdict_(toKernel(verdict))
{
}

std::size_t StandardTarget::size() const
{
    return RuleLayout::standardTargetSize();
}

void StandardTarget::write(unsigned char* ptr) const
{
    RuleLayout::writeStandardTarget(ptr, verdict_);
}

int StandardTarget::toKernel(Verdict verdict)
{
    switch (verdict)
    {
        case Verdict::ACCEPT:
            return -NF_ACCEPT - 1;
        case Verdict::DROP:
            return -NF_DROP - 1;
        case Verdict::RETURN:
            return XT_RETURN;
    }
    return -NF_DROP - 1;
}
