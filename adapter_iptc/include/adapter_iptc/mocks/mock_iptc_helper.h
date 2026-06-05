#ifndef MOCK_IPTC_HELPER_H
#define MOCK_IPTC_HELPER_H

#include <gmock/gmock.h>

#include "adapter_iptc/i_iptc_helper.h"

class MockIptcHelper : public IIptcHelper {
public:
  MOCK_METHOD0(createChain, void());
  MOCK_METHOD0(flushEntries, void());
  MOCK_METHOD1(appendEntry, void(const ipt_entry *entry));
  MOCK_METHOD0(commit, void());
};

#endif
