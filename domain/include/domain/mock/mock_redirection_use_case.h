#pragma once

#include "domain/use_case/interface_redirection_use_case.h"

#include <gmock/gmock.h>

namespace domain {

class MockRedirectionUseCase : public IRedirectionUseCase
{
public:
    MOCK_METHOD((std::expected<void, RedirectionProblem>), execute,
                (const RedirectionRequest& request), (override));
};

} // namespace domain
