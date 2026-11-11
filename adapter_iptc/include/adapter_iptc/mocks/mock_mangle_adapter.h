#ifndef MOCK_MANGLE_ADAPTER_H
#define MOCK_MANGLE_ADAPTER_H

#include "adapter_iptc/i_mangle_adapter.h"

#include <gmock/gmock.h>
#include <vector>

class MockMangleAdapter : public IMangleAdapter
{
public:
    MOCK_METHOD0(createChain, void());
    MOCK_METHOD0(flush, void());
    MOCK_METHOD2(addConntrackRule, void(MarkParam mark, ConntrackParam conntrack));
    MOCK_METHOD2(addNflogRule, void(MarkParam mark, NflogParam nflog));
    MOCK_METHOD0(commit, void());
    MOCK_METHOD0(readNflogRules, std::vector<NflogRuleParam>());
};

#endif
