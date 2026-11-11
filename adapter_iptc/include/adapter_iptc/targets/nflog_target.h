#ifndef NFLOG_TARGET_H
#define NFLOG_TARGET_H

#include "adapter_iptc/dtos/nflog.h"
#include "adapter_iptc/targets/extension_target_template.h"

#include <cstdint>
#include <optional>
#include <string>

struct ipt_entry;

class NflogTarget : public ExtensionTargetTemplate
{
public:
    NflogTarget(uint16_t group, const std::string& prefix);
    explicit NflogTarget(NflogParam nflog);

    /* LEARN: decodes an entry read back from the kernel; nullopt when its target is not NFLOG. */
    [[nodiscard]] static std::optional<NflogParam> read(const ipt_entry& entry);

protected:
    const char* name() const override;
    std::size_t dataSize() const override;
    void fillData(unsigned char* data) const override;

private:
    /* LEARN: finds the NFLOG payload (xt_nflog_info) inside one rule buffer. Returns nullptr
       when the rule's target is not NFLOG, or when the offsets and sizes stored in the buffer
       do not fit inside it (see the layout drawn at the definition).
    */
    static const unsigned char* dataOf(const ipt_entry& entry);

    NflogParam m_nflog;
};

#endif
