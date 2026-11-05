#include "adapter_socket/peer_connection/helper_config_dto.h"

#include "parameterized_case.h"

#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vector>

constexpr std::string_view CONFIGURED_USER = "root";
constexpr std::string_view NO_USER = "";
constexpr std::string_view PLAIN_CONFIG = "agent_user=root\n";
constexpr std::string_view PADDED_CONFIG =
    "# bellerophon helper\n\n   agent_user   =   root   \nunrelated_key=value\n";
constexpr std::string_view MISSING_KEY_CONFIG = "unrelated_key=root\n";
constexpr std::string_view EMPTY_VALUE_CONFIG = "agent_user=\n";
constexpr std::string_view NO_TEXT = "";

struct ConfigTextCase
{
    std::string_view text;
    std::string_view expectedAgentUser;
    const char* name;
};

const std::vector<ConfigTextCase> CONFIG_TEXTS{
    {PLAIN_CONFIG, CONFIGURED_USER, "PlainLine"},
    {PADDED_CONFIG, CONFIGURED_USER, "PaddedWithComments"},
    {MISSING_KEY_CONFIG, NO_USER, "MissingKey"},
    {EMPTY_VALUE_CONFIG, NO_USER, "EmptyValue"},
    {NO_TEXT, NO_USER, "NoText"},
};

class HelperConfigDtoTest : public ::testing::TestWithParam<ConfigTextCase>
{
};

TEST_P(HelperConfigDtoTest, ParseYieldsExpectedAgentUser)
{
    const std::string text(GetParam().text);
    const adapter_socket::HelperConfigDto config = adapter_socket::HelperConfigDto::parse(text);

    EXPECT_EQ(config.agentUser, GetParam().expectedAgentUser);
}

INSTANTIATE_TEST_SUITE_P(ConfigText, HelperConfigDtoTest, ::testing::ValuesIn(CONFIG_TEXTS),
                         caseName<ConfigTextCase>);
