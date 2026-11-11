#ifndef I_IPTC_BUILDER_H
#define I_IPTC_BUILDER_H

#include <memory>

struct ipt_entry;

class IBuilder
{
public:
    virtual ~IBuilder() = default;
    virtual std::shared_ptr<ipt_entry> build() const = 0;
};

#endif
