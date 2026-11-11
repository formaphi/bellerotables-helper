#ifndef HASHLIMIT_MATCH_H
#define HASHLIMIT_MATCH_H

#include "adapter_iptc/template_match.h"

#include <cstdint>
#include <string>

class HashlimitMatch : public TemplateMatch
{
public:
    HashlimitMatch(std::string hashlimitName, uint32_t average, uint32_t burst, uint32_t expire,
                   uint32_t garbageCollectionInterval = 1000, uint8_t sourceMask = 32,
                   uint8_t destinationMask = 32, uint32_t mode = 0);

protected:
    [[nodiscard]] const char* name() const override;
    [[nodiscard]] uint8_t revision() const override;
    [[nodiscard]] std::size_t dataSize() const override;
    void fillData(unsigned char* data) const override;

private:
    std::string m_hashlimitName;
    uint32_t m_average;
    uint32_t m_burst;
    uint32_t m_expire;
    uint32_t m_garbageCollectionInterval;
    uint8_t m_sourceMask;
    uint8_t m_destinationMask;
    uint32_t m_mode;
};

#endif
