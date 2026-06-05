#include "adapter_iptc/targets/standard_target.h"

#include <linux/netfilter.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter_ipv4/ip_tables.h>

StandardTarget::StandardTarget(Verdict v) : verdict_(toKernel(v)) {}

std::size_t StandardTarget::size() const {
    return XT_ALIGN(sizeof(xt_standard_target));
}

void StandardTarget::write(unsigned char *ptr) const {
    auto *st = reinterpret_cast<xt_standard_target *>(ptr);
    st->target.u.user.target_size = size();
    st->verdict = verdict_;
}

int StandardTarget::toKernel(Verdict v) {
    switch (v) {
    case Verdict::ACCEPT:
        return -NF_ACCEPT - 1;
    case Verdict::DROP:
        return -NF_DROP - 1;
    case Verdict::RETURN:
        return XT_RETURN;
    }
    return -NF_DROP - 1;
}
