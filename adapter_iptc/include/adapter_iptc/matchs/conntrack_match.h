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
    /* LEARN: the two values State stands for in xt_conntrack_mtinfo3. */
    struct Flags
    {
        uint16_t matchFlags;
        uint16_t stateMask;
    };

    Flags m_flags;

    [[nodiscard]] static Flags resolve(State state);
};

#endif
