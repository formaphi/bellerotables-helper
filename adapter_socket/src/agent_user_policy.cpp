#include "adapter_socket/peer_connection/agent_user_policy.h"

#include "adapter_socket/peer_connection/helper_config_dto.h"

#include <fstream>
#include <sstream>
#include <string>

namespace adapter_socket {

std::string AgentUserPolicy::readFile(const std::string& path)
{
    std::ifstream file(path);
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

AgentUserPolicy::AgentUserPolicy(std::unique_ptr<IUserDirectory> userDirectory,
                                 const std::string& configPath)
{
    const std::string configText = readFile(configPath);
    const HelperConfigDto config = HelperConfigDto::parse(configText);
    const bool hasAgentUser = !config.agentUser.empty();
    if (hasAgentUser)
    {
        m_allowedUserId = userDirectory->findUserId(config.agentUser);
    }
}

bool AgentUserPolicy::isAllowed(const PeerCredentials& peer) const
{
    /* LEARN: UNKNOWN_ID is what a PeerCredentials holds when the kernel never filled it, and it is
       also uid 0 (root). A peer with that uid is always refused, before the configured user is
       even compared, so an unread or zeroed credential can never authorize a connection that
       changes firewall rules. As a consequence agent_user=root never authorizes anyone: the agent
       must run as its own unprivileged user.
    */
    const bool isUnknownPeer = peer.userId == PeerCredentials::UNKNOWN_ID;
    if (isUnknownPeer)
    {
        return false;
    }

    return m_allowedUserId.has_value() && *m_allowedUserId == peer.userId;
}

} // namespace adapter_socket
