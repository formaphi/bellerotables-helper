#ifndef MOCK_BUILDER_H
#define MOCK_BUILDER_H

#include <gmock/gmock.h>

#include "adapter_iptc/i_iptc_builder.h"

class MockBuilder : public IBuilder {
public:
  MOCK_CONST_METHOD0(build, std::shared_ptr<ipt_entry>());
};

#endif
