#ifndef STANDARD_TARGET_H
#define STANDARD_TARGET_H

#include "adapter_iptc/i_target_strategy.h"

#include <cstdint>

class StandardTarget : public ITargetStrategy
{
public:
    enum class Verdict : uint8_t
    {
        ACCEPT,
        DROP,
        RETURN
    };

    explicit StandardTarget(Verdict verdict);

    [[nodiscard]] std::size_t size() const override;
    void write(unsigned char* ptr) const override;

private:
    int verdict_;

    static int toKernel(Verdict verdict);
};

#endif
