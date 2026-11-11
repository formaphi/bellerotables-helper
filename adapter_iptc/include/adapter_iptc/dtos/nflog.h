#ifndef NFLOG_DTO_H
#define NFLOG_DTO_H

#include "adapter_iptc/dtos/mark.h"

#include <cstdint>
#include <optional>
#include <string>

/* LEARN: the NFLOG knobs in kernel widths (xt_nflog_info).
   prefix     --nflog-prefix; the kernel keeps at most 63 bytes, cut on a UTF-8 character boundary.
   group      --nflog-group.
   range      bytes copied to userspace. Written as len + XT_NFLOG_F_COPY_LEN, the encoding of
              --nflog-size: the kernel ignores len without the flag, which is why iptables
              deprecated --nflog-range ("has never worked").
   threshold  --nflog-threshold; 0 is the kernel default, so a threshold of 0 reads back as unset.
*/
struct NflogParam
{
    std::string prefix;
    uint16_t group;
    std::optional<uint32_t> range;
    std::optional<uint16_t> threshold;

    bool operator==(const NflogParam&) const = default;
};

/* LEARN: one NFLOG rule read back from the kernel: the same pair addNflogRule() takes. mark is
   empty when the rule has no plain mark match (none, or one with a mask or an inversion that
   MarkParam cannot express), so such a rule never reads as one the adapter wrote.
*/
struct NflogRuleParam
{
    std::optional<MarkParam> mark;
    NflogParam nflog;

    bool operator==(const NflogRuleParam&) const = default;
};

#endif
