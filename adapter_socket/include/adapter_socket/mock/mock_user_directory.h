#pragma once

#include "adapter_socket/user_directory/interface_user_directory.h"

#include <gmock/gmock.h>

namespace adapter_socket {

class MockUserDirectory : public IUserDirectory
{
public:
    MOCK_METHOD(std::optional<uint32_t>, findUserId, (const std::string& userName),
                (const, override));
};

} // namespace adapter_socket
