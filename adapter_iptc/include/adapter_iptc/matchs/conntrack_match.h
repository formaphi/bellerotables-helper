#ifndef CONNTRACK_MATCH_H
#define CONNTRACK_MATCH_H

#include "adapter_iptc/template_match.h"

#include <cstdint>

class ConntrackMatch : public TemplateMatch
{
public:
    enum class State : uint8_t
    {
        NEW,
        ESTABLISHED_RELATED,
        INVALID
    };

    explicit ConntrackMatch(State state);

protected:
    [[nodiscard]] const char* name() const override;
    [[nodiscard]] uint8_t revision() const override;
    [[nodiscard]] std::size_t dataSize() const override;
    void fillData(unsigned char* data) const override;

private:
    uint16_t m_matchFlags{};
    uint16_t m_stateMask{};

    static void resolve(State state, uint16_t& matchFlags, uint16_t& stateMask);
};

#endif
