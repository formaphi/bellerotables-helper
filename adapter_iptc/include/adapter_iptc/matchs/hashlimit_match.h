#ifndef HASHLIMIT_MATCH_H
#define HASHLIMIT_MATCH_H

#include "adapter_iptc/dtos/hashlimit.h"
#include "adapter_iptc/template_match.h"

#include <cstdint>
#include <string>

class HashlimitMatch : public TemplateMatch
{
public:
    explicit HashlimitMatch(HashlimitParam hashlimit);

protected:
    [[nodiscard]] const char* name() const override;
    [[nodiscard]] uint8_t revision() const override;
    [[nodiscard]] std::size_t dataSize() const override;
    void fillData(unsigned char* data) const override;

private:
    HashlimitParam m_hashlimit;
};

#endif
