#include <adapter_socket/peer_connection/helper_config_dto.h>
#include <gtest/gtest.h>
#include <string>
#include <string_view>

constexpr std::string_view CONFIGURED_USER = "root";
constexpr std::string_view PLAIN_CONFIG = "agent_user=root\n";
constexpr std::string_view PADDED_CONFIG =
    "# bellerophon helper\n\n   agent_user   =   root   \nunrelated_key=value\n";
constexpr std::string_view MISSING_KEY_CONFIG = "unrelated_key=root\n";
constexpr std::string_view EMPTY_VALUE_CONFIG = "agent_user=\n";
constexpr std::string_view NO_TEXT = "";

TEST(HelperConfigDtoTest, PlainLineYieldsAgentUser)
{
    const std::string text(PLAIN_CONFIG);
    const adapter_socket::HelperConfigDto config = adapter_socket::HelperConfigDto::parse(text);

    EXPECT_EQ(config.agentUser, CONFIGURED_USER);
}

TEST(HelperConfigDtoTest, PaddedConfigWithCommentsYieldsTrimmedAgentUser)
{
    const std::string text(PADDED_CONFIG);
    const adapter_socket::HelperConfigDto config = adapter_socket::HelperConfigDto::parse(text);

    EXPECT_EQ(config.agentUser, CONFIGURED_USER);
}

TEST(HelperConfigDtoTest, MissingKeyYieldsEmptyAgentUser)
{
    const std::string text(MISSING_KEY_CONFIG);
    const adapter_socket::HelperConfigDto config = adapter_socket::HelperConfigDto::parse(text);

    EXPECT_TRUE(config.agentUser.empty());
}

TEST(HelperConfigDtoTest, EmptyValueYieldsEmptyAgentUser)
{
    const std::string text(EMPTY_VALUE_CONFIG);
    const adapter_socket::HelperConfigDto config = adapter_socket::HelperConfigDto::parse(text);

    EXPECT_TRUE(config.agentUser.empty());
}

TEST(HelperConfigDtoTest, NoTextYieldsEmptyAgentUser)
{
    const std::string text(NO_TEXT);
    const adapter_socket::HelperConfigDto config = adapter_socket::HelperConfigDto::parse(text);

    EXPECT_TRUE(config.agentUser.empty());
}
