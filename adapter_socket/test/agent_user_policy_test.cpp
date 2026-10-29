#include <adapter_socket/peer_connection/agent_user_policy.h>

#include <adapter_socket/peer_connection/peer_credentials.h>

#include <gtest/gtest.h>

#include <unistd.h>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

constexpr uint32_t ROOT_USER_ID = 0;
constexpr uint32_t UNRELATED_USER_ID = 1000;
constexpr adapter_socket::PeerCredentials ROOT_PEER{ROOT_USER_ID};
constexpr adapter_socket::PeerCredentials UNRELATED_PEER{UNRELATED_USER_ID};
constexpr std::string_view TEMP_DIRECTORY_TEMPLATE = "/tmp/adapter_socket_policy_XXXXXX";
constexpr std::string_view CONFIG_FILE_NAME = "bellerotables-helper.conf";
constexpr std::string_view MISSING_FILE_NAME = "missing.conf";
constexpr std::string_view PLAIN_CONFIG = "agent_user=root\n";
constexpr std::string_view PADDED_CONFIG =
    "# bellerophon helper\n\n   agent_user   =   root   \nunrelated_key=value\n";
constexpr std::string_view MISSING_KEY_CONFIG = "unrelated_key=root\n";
constexpr std::string_view EMPTY_VALUE_CONFIG = "agent_user=\n";
constexpr std::string_view UNKNOWN_USER_CONFIG = "agent_user=no-such-user-bellerophon\n";

class AgentUserPolicyTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        std::string directoryTemplate(TEMP_DIRECTORY_TEMPLATE);
        const char* const createdDirectory = ::mkdtemp(directoryTemplate.data());
        if (createdDirectory == nullptr)
        {
            const int savedErrno = errno;
            throw std::system_error(savedErrno, std::generic_category(), "mkdtemp");
        }
        m_directory = directoryTemplate;
    }

    void TearDown() override
    {
        ::unlink(configPath().c_str());
        ::rmdir(m_directory.c_str());
    }

    std::string configPath() const
    {
        return m_directory + "/" + std::string(CONFIG_FILE_NAME);
    }

    std::string missingConfigPath() const
    {
        return m_directory + "/" + std::string(MISSING_FILE_NAME);
    }

    std::string writeConfig(std::string_view contents) const
    {
        const std::string path = configPath();
        std::ofstream stream(path);
        stream << contents;
        return path;
    }

    std::string m_directory;
};

TEST_F(AgentUserPolicyTest, ResolvesConfiguredUserAndAllowsOnlyItsUserId)
{
    const std::string configFile = writeConfig(PLAIN_CONFIG);
    const adapter_socket::AgentUserPolicy policy(configFile);

    EXPECT_TRUE(policy.isAllowed(ROOT_PEER));
    EXPECT_FALSE(policy.isAllowed(UNRELATED_PEER));
}

TEST_F(AgentUserPolicyTest, ToleratesWhitespaceCommentsAndBlankLines)
{
    const std::string configFile = writeConfig(PADDED_CONFIG);
    const adapter_socket::AgentUserPolicy policy(configFile);

    EXPECT_TRUE(policy.isAllowed(ROOT_PEER));
    EXPECT_FALSE(policy.isAllowed(UNRELATED_PEER));
}

TEST_F(AgentUserPolicyTest, MissingConfigFileFailsClosed)
{
    const std::string missingFile = missingConfigPath();
    const adapter_socket::AgentUserPolicy policy(missingFile);

    EXPECT_FALSE(policy.isAllowed(ROOT_PEER));
}

TEST_F(AgentUserPolicyTest, MissingKeyFailsClosed)
{
    const std::string configFile = writeConfig(MISSING_KEY_CONFIG);
    const adapter_socket::AgentUserPolicy policy(configFile);

    EXPECT_FALSE(policy.isAllowed(ROOT_PEER));
}

TEST_F(AgentUserPolicyTest, EmptyValueFailsClosed)
{
    const std::string configFile = writeConfig(EMPTY_VALUE_CONFIG);
    const adapter_socket::AgentUserPolicy policy(configFile);

    EXPECT_FALSE(policy.isAllowed(ROOT_PEER));
}

TEST_F(AgentUserPolicyTest, UnknownUserFailsClosed)
{
    const std::string configFile = writeConfig(UNKNOWN_USER_CONFIG);
    const adapter_socket::AgentUserPolicy policy(configFile);

    EXPECT_FALSE(policy.isAllowed(ROOT_PEER));
}
