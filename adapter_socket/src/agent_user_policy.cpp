#include <adapter_socket/peer_connection/agent_user_policy.h>
#include <adapter_socket/peer_connection/helper_config_dto.h>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <optional>
#include <pwd.h>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace adapter_socket {

constexpr long SYSCONF_INDETERMINATE = -1;
constexpr long FALLBACK_PASSWORD_BUFFER_BYTES = 16384;
constexpr int GETPWNAM_SUCCESS = 0;
constexpr std::size_t MAX_PASSWORD_BUFFER_BYTES = 1048576;
constexpr std::size_t BUFFER_GROWTH_FACTOR = 2;

std::string AgentUserPolicy::readFile(const std::string& path)
{
    std::ifstream file(path);
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

std::optional<uint32_t> AgentUserPolicy::resolveUserId(const std::string& userName)
{
    long bufferSize = ::sysconf(_SC_GETPW_R_SIZE_MAX);
    if (bufferSize == SYSCONF_INDETERMINATE)
    {
        bufferSize = FALLBACK_PASSWORD_BUFFER_BYTES;
    }

    std::vector<char> buffer(static_cast<std::size_t>(bufferSize));
    passwd entry{};
    passwd* found = nullptr;
    int lookupResult = GETPWNAM_SUCCESS;
    while (true)
    {
        lookupResult = ::getpwnam_r(userName.c_str(), &entry, buffer.data(), buffer.size(), &found);
        /*LEARN:
         * getpwnam_r looks up the agent's uid in the system user database (NSS, usually
         * /etc/passwd). The caller supplies the memory for the result strings. ERANGE here means
         * the buffer is too small for the result. The needed size is unknown beforehand, and
         * sysconf only suggests a starting size. So the loop doubles the buffer and retries, up to
         * a cap.
         */
        const bool bufferTooSmall =
            lookupResult == ERANGE && buffer.size() < MAX_PASSWORD_BUFFER_BYTES;
        if (!bufferTooSmall)
        {
            break;
        }
        buffer.resize(buffer.size() * BUFFER_GROWTH_FACTOR);
    }

    if (lookupResult != GETPWNAM_SUCCESS || found == nullptr)
    {
        return std::nullopt;
    }

    return static_cast<uint32_t>(found->pw_uid);
}

AgentUserPolicy::AgentUserPolicy(const std::string& configPath)
{
    const std::string configText = readFile(configPath);
    const HelperConfigDto config = HelperConfigDto::parse(configText);
    const bool hasAgentUser = !config.agentUser.empty();
    if (hasAgentUser)
    {
        m_allowedUserId = resolveUserId(config.agentUser);
    }
}

bool AgentUserPolicy::isAllowed(const PeerCredentials& peer) const
{
    return m_allowedUserId.has_value() && *m_allowedUserId == peer.userId;
}

} // namespace adapter_socket
