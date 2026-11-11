#ifndef NFLOG_TARGET_H
#define NFLOG_TARGET_H

#include "adapter_iptc/targets/extension_target_template.h"

#include <cstdint>
#include <string>

class NflogTarget : public ExtensionTargetTemplate
{
public:
    NflogTarget(uint16_t group, const std::string& prefix);

protected:
    const char* name() const override;
    std::size_t dataSize() const override;
    void fillData(unsigned char* data) const override;

private:
    uint16_t m_group;
    std::string m_prefix;
};

#endif
