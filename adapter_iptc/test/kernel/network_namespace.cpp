#include "network_namespace.h"

#include <cerrno>
#include <fstream>
#include <sched.h>
#include <string>
#include <system_error>
#include <unistd.h>

namespace {

bool writeProcFile(const std::string& path, const std::string& content)
{
    std::ofstream file(path);
    file << content;
    file.flush();
    return static_cast<bool>(file);
}

std::string describe(const char* call, int errorNumber)
{
    return std::string(call) + ": " + std::generic_category().message(errorNumber);
}

} // namespace

std::string enterIsolatedNetworkNamespace()
{
    const uid_t userId = geteuid();
    const gid_t groupId = getegid();

    if (unshare(CLONE_NEWUSER | CLONE_NEWNET) == 0)
    {
        const bool mapped =
            writeProcFile("/proc/self/setgroups", "deny") &&
            writeProcFile("/proc/self/uid_map", "0 " + std::to_string(userId) + " 1") &&
            writeProcFile("/proc/self/gid_map", "0 " + std::to_string(groupId) + " 1");
        return mapped ? std::string() : std::string("writing the user namespace id maps failed");
    }
    const std::string userNamespaceFailure =
        describe("unshare(CLONE_NEWUSER | CLONE_NEWNET)", errno);

    if (unshare(CLONE_NEWNET) == 0)
    {
        return {};
    }
    return userNamespaceFailure + "; " + describe("unshare(CLONE_NEWNET)", errno);
}
