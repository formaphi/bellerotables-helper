#ifndef MOCK_MANGLE_ADAPTER_H
#define MOCK_MANGLE_ADAPTER_H

#include <gmock/gmock.h>

#include "adapter_iptc/i_mangle_adapter.h"

class MockMangleAdapter : public IMangleAdapter {
public:
  MOCK_METHOD0(createChain, void());
  MOCK_METHOD0(flush, void());
  MOCK_METHOD2(addConntrackRule, void(MarkParam mark, ConntrackParam conntrack));
  MOCK_METHOD2(addNflogRule, void(MarkParam mark, NflogParam nflog));
  MOCK_METHOD0(commit, void());
};

#endif
