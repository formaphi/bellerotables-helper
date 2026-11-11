#ifndef STANDARD_TARGET_H
#define STANDARD_TARGET_H

#include "adapter_iptc/i_target_strategy.h"

class StandardTarget : public ITargetStrategy
{
public:
    enum class Verdict
    {
        ACCEPT,
        DROP,
        RETURN
    };

    explicit StandardTarget(Verdict v);

    std::size_t size() const override;
    void write(unsigned char* ptr) const override;

private:
    int verdict_;

    static int toKernel(Verdict v);
};

#endif
