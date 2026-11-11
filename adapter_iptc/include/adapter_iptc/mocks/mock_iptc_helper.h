#ifndef MOCK_IPTC_HELPER_H
#define MOCK_IPTC_HELPER_H

#include "adapter_iptc/i_iptc_helper.h"

#include <gmock/gmock.h>
#include <vector>

class MockIptcHelper : public IIptcHelper
{
public:
    MOCK_METHOD0(createChain, void());
    MOCK_METHOD0(flushEntries, void());
    MOCK_METHOD1(appendEntry, void(const ipt_entry* entry));
    MOCK_METHOD0(commit, void());
    MOCK_METHOD0(chainExists, bool());
    MOCK_METHOD0(listEntries, std::vector<IptEntryPtr>());
};

#endif
