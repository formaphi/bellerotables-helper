#include "adapter_iptc/targets/standard_target.h"

#include <linux/netfilter.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter_ipv4/ip_tables.h>

StandardTarget::StandardTarget(Verdict verdict)
    : verdict_(toKernel(verdict))
{
}

std::size_t StandardTarget::size() const
{
    return XT_ALIGN(sizeof(xt_standard_target));
}

void StandardTarget::write(unsigned char* ptr) const
{
    auto* standardTarget = reinterpret_cast<xt_standard_target*>(ptr);
    standardTarget->target.u.user.target_size = size();
    standardTarget->verdict = verdict_;
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
