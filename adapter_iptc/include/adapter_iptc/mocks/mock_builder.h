#ifndef MOCK_BUILDER_H
#define MOCK_BUILDER_H

#include "adapter_iptc/i_iptc_builder.h"
#include "adapter_iptc/ipt_entry_ptr.h"

#include <gmock/gmock.h>

class MockBuilder : public IBuilder
{
public:
    MOCK_CONST_METHOD0(build, IptEntryPtr());
};

#endif
