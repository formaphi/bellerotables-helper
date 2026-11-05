#include "adapter_socket/user_directory/posix_user_directory.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <optional>

constexpr uint32_t ROOT_USER_ID = 0;
constexpr const char* ROOT_USER_NAME = "root";
constexpr const char* UNKNOWN_USER_NAME = "no-such-user-bellerophon";

TEST(PosixUserDirectoryTest, FindsUserIdOfExistingUser)
{
    const adapter_socket::PosixUserDirectory directory;

    const std::optional<uint32_t> userId = directory.findUserId(ROOT_USER_NAME);

    ASSERT_TRUE(userId.has_value());
    EXPECT_EQ(userId.value(), ROOT_USER_ID);
}

TEST(PosixUserDirectoryTest, UnknownUserIsNotFound)
{
    const adapter_socket::PosixUserDirectory directory;

    EXPECT_FALSE(directory.findUserId(UNKNOWN_USER_NAME).has_value());
}
