#ifndef MOCK_TARGET_STRATEGY_H
#define MOCK_TARGET_STRATEGY_H

#include "adapter_iptc/i_target_strategy.h"

#include <gmock/gmock.h>

class MockTargetStrategy : public ITargetStrategy
{
public:
    MOCK_CONST_METHOD0(size, std::size_t());
    MOCK_CONST_METHOD1(write, void(unsigned char* ptr));
};

#endif
