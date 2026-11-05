#include "adapter_socket/peer_connection/agent_user_policy.h"

#include "adapter_socket/mock/mock_user_directory.h"
#include "adapter_socket/peer_connection/peer_credentials.h"
#include "parameterized_case.h"

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

using adapter_socket::MockUserDirectory;
using adapter_socket::PeerCredentials;
using ::testing::_;
using ::testing::Return;

constexpr uint32_t AGENT_USER_ID = 900;
constexpr uint32_t UNRELATED_USER_ID = 1000;
constexpr uint32_t AGENT_GROUP_ID = 901;
constexpr uint32_t AGENT_PROCESS_ID = 4242;
constexpr PeerCredentials UNREAD_PEER{};
constexpr PeerCredentials UNKNOWN_USER_WITH_VALID_GROUP_AND_PROCESS{
    PeerCredentials::UNKNOWN_ID, AGENT_GROUP_ID, AGENT_PROCESS_ID};
constexpr const char* ROOT_USER_NAME = "root";
constexpr std::string_view ROOT_CONFIG = "agent_user=root\n";
constexpr adapter_socket::PeerCredentials AGENT_PEER{AGENT_USER_ID};
constexpr adapter_socket::PeerCredentials UNRELATED_PEER{UNRELATED_USER_ID};
constexpr const char* AGENT_USER_NAME = "bellero-agent";
constexpr const char* UNKNOWN_USER_NAME = "no-such-user-bellerophon";
constexpr std::string_view TEMP_DIRECTORY_TEMPLATE = "/tmp/adapter_socket_policy_XXXXXX";
constexpr std::string_view CONFIG_FILE_NAME = "bellerotables-helper.conf";
constexpr std::string_view MISSING_FILE_NAME = "missing.conf";
constexpr std::string_view PLAIN_CONFIG = "agent_user=bellero-agent\n";
constexpr std::string_view PADDED_CONFIG =
    "# bellerophon helper\n\n   agent_user   =   bellero-agent   \nunrelated_key=value\n";
constexpr std::string_view MISSING_KEY_CONFIG = "unrelated_key=bellero-agent\n";
constexpr std::string_view EMPTY_VALUE_CONFIG = "agent_user=\n";
constexpr std::string_view UNKNOWN_USER_CONFIG = "agent_user=no-such-user-bellerophon\n";

class AgentUserPolicyTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        std::unique_ptr<MockUserDirectory> userDirectory = std::make_unique<MockUserDirectory>();
        m_userDirectory = userDirectory.get();
        m_ownedUserDirectory = std::move(userDirectory);

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
    std::unique_ptr<MockUserDirectory> m_ownedUserDirectory;
    MockUserDirectory* m_userDirectory = nullptr;
};

TEST_F(AgentUserPolicyTest, MissingConfigFileFailsClosed)
{
    EXPECT_CALL(*m_userDirectory, findUserId(_)).Times(0);
    const std::string missingFile = missingConfigPath();
    const adapter_socket::AgentUserPolicy policy(std::move(m_ownedUserDirectory), missingFile);

    EXPECT_FALSE(policy.isAllowed(AGENT_PEER));
}

TEST_F(AgentUserPolicyTest, UnknownUserFailsClosed)
{
    EXPECT_CALL(*m_userDirectory, findUserId(std::string(UNKNOWN_USER_NAME)))
        .WillOnce(Return(std::nullopt));
    const std::string configFile = writeConfig(UNKNOWN_USER_CONFIG);
    const adapter_socket::AgentUserPolicy policy(std::move(m_ownedUserDirectory), configFile);

    EXPECT_FALSE(policy.isAllowed(AGENT_PEER));
    EXPECT_FALSE(policy.isAllowed(UNRELATED_PEER));
}

TEST_F(AgentUserPolicyTest, RejectsUnreadPeerButStillAllowsConfiguredAgent)
{
    EXPECT_CALL(*m_userDirectory, findUserId(std::string(AGENT_USER_NAME)))
        .WillOnce(Return(AGENT_USER_ID));
    const std::string configFile = writeConfig(PLAIN_CONFIG);
    const adapter_socket::AgentUserPolicy policy(std::move(m_ownedUserDirectory), configFile);

    EXPECT_FALSE(policy.isAllowed(UNREAD_PEER));
    EXPECT_TRUE(policy.isAllowed(AGENT_PEER));
}

struct ConfigCase
{
    std::string_view text;
    const char* name;
};

struct PeerCase
{
    PeerCredentials peer;
    const char* name;
};

const std::vector<ConfigCase> AGENT_CONFIGS{
    {PLAIN_CONFIG, "PlainLine"},
    {PADDED_CONFIG, "WhitespaceCommentsAndBlankLines"},
};

const std::vector<ConfigCase> CONFIGS_WITHOUT_AGENT{
    {MISSING_KEY_CONFIG, "MissingKey"},
    {EMPTY_VALUE_CONFIG, "EmptyValue"},
};

const std::vector<PeerCase> UNKNOWN_ID_PEERS{
    {UNREAD_PEER, "UnreadPeer"},
    {UNKNOWN_USER_WITH_VALID_GROUP_AND_PROCESS, "UnknownUserWithValidGroupAndProcess"},
};

class AgentUserPolicyAgentConfigTest : public AgentUserPolicyTest,
                                       public ::testing::WithParamInterface<ConfigCase>
{
};

class AgentUserPolicyConfigWithoutAgentTest : public AgentUserPolicyTest,
                                              public ::testing::WithParamInterface<ConfigCase>
{
};

class AgentUserPolicyUnknownIdTest : public AgentUserPolicyTest,
                                     public ::testing::WithParamInterface<PeerCase>
{
};

TEST_P(AgentUserPolicyAgentConfigTest, AllowsOnlyTheConfiguredAgent)
{
    EXPECT_CALL(*m_userDirectory, findUserId(std::string(AGENT_USER_NAME)))
        .WillOnce(Return(AGENT_USER_ID));
    const std::string configFile = writeConfig(GetParam().text);
    const adapter_socket::AgentUserPolicy policy(std::move(m_ownedUserDirectory), configFile);

    EXPECT_TRUE(policy.isAllowed(AGENT_PEER));
    EXPECT_FALSE(policy.isAllowed(UNRELATED_PEER));
}

TEST_P(AgentUserPolicyConfigWithoutAgentTest, FailsClosedWithoutLookingUpAUser)
{
    EXPECT_CALL(*m_userDirectory, findUserId(_)).Times(0);
    const std::string configFile = writeConfig(GetParam().text);
    const adapter_socket::AgentUserPolicy policy(std::move(m_ownedUserDirectory), configFile);

    EXPECT_FALSE(policy.isAllowed(AGENT_PEER));
}

TEST_P(AgentUserPolicyUnknownIdTest, IsRejectedEvenWhenAgentUserResolvesToUnknownId)
{
    EXPECT_CALL(*m_userDirectory, findUserId(std::string(ROOT_USER_NAME)))
        .WillOnce(Return(PeerCredentials::UNKNOWN_ID));
    const std::string configFile = writeConfig(ROOT_CONFIG);
    const adapter_socket::AgentUserPolicy policy(std::move(m_ownedUserDirectory), configFile);

    EXPECT_FALSE(policy.isAllowed(GetParam().peer));
}

INSTANTIATE_TEST_SUITE_P(Config, AgentUserPolicyAgentConfigTest, ::testing::ValuesIn(AGENT_CONFIGS),
                         caseName<ConfigCase>);

INSTANTIATE_TEST_SUITE_P(Config, AgentUserPolicyConfigWithoutAgentTest,
                         ::testing::ValuesIn(CONFIGS_WITHOUT_AGENT), caseName<ConfigCase>);

INSTANTIATE_TEST_SUITE_P(Peer, AgentUserPolicyUnknownIdTest, ::testing::ValuesIn(UNKNOWN_ID_PEERS),
                         caseName<PeerCase>);
