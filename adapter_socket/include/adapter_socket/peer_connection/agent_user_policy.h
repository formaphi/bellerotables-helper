#ifndef ADAPTER_SOCKET_AGENT_USER_POLICY_H
#define ADAPTER_SOCKET_AGENT_USER_POLICY_H

#include "adapter_socket/peer_connection/interface_peer_policy.h"
#include "adapter_socket/user_directory/interface_user_directory.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace adapter_socket {

class AgentUserPolicy final : public IPeerPolicy
{
public:
    static constexpr const char* DEFAULT_CONFIG_PATH = "/etc/bellerophon/bellerotables-helper.conf";

    explicit AgentUserPolicy(std::unique_ptr<IUserDirectory> userDirectory,
                             const std::string& configPath = DEFAULT_CONFIG_PATH);
    ~AgentUserPolicy() override = default;

    AgentUserPolicy(const AgentUserPolicy&) = delete;
    AgentUserPolicy& operator=(const AgentUserPolicy&) = delete;
    AgentUserPolicy(AgentUserPolicy&&) = delete;
    AgentUserPolicy& operator=(AgentUserPolicy&&) = delete;

    [[nodiscard]] bool isAllowed(const PeerCredentials& peer) const override;

private:
    [[nodiscard]] static std::string readFile(const std::string& path);

    std::optional<uint32_t> m_allowedUserId;
};

} // namespace adapter_socket

#endif /* ADAPTER_SOCKET_AGENT_USER_POLICY_H */
