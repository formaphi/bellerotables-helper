#include "adapter_socket/systemd_daemon/libsystemd_daemon.h"

#include <cstddef>
#include <sys/socket.h>
#include <system_error>
#include <systemd/sd-daemon.h>
#include <utility>
#include <vector>

namespace adapter_socket {

constexpr int UNSET_ACTIVATION_ENVIRONMENT = 1;
constexpr int MUST_BE_LISTENING = 1;
constexpr int NOT_A_MATCH = 0;
constexpr int NO_ERROR_RESULT = 0;
constexpr int FIRST_OFFSET = 0;
constexpr std::size_t NO_PATH_LENGTH = 0;

namespace {

std::error_code fromNegatedErrno(int negatedErrno)
{
    return {-negatedErrno, std::generic_category()};
}

} // namespace

SyscallResult<std::vector<int>> LibsystemdDaemon::takeListenFds() const
{
    /* LEARN: sd_listen_fds reports how many sockets systemd passed, numbered from
       SD_LISTEN_FDS_START (3). Passing 1 unsets LISTEN_PID / LISTEN_FDS, so a child process does
       not believe it was activated too. libsystemd returns failures as a negative errno.
    */
    const int activatedCount = ::sd_listen_fds(UNSET_ACTIVATION_ENVIRONMENT);
    if (activatedCount < NO_ERROR_RESULT)
    {
        return SyscallResult<std::vector<int>>::failure(fromNegatedErrno(activatedCount));
    }

    std::vector<int> listenFds;
    listenFds.reserve(static_cast<std::size_t>(activatedCount));
    for (int offset = FIRST_OFFSET; offset < activatedCount; ++offset)
    {
        listenFds.push_back(SD_LISTEN_FDS_START + offset);
    }
    return SyscallResult<std::vector<int>>::success(std::move(listenFds));
}

SyscallResult<bool> LibsystemdDaemon::isListeningUnixStream(int fileDescriptor) const
{
    const int isListeningUnixStream = ::sd_is_socket_unix(
        fileDescriptor, SOCK_STREAM, MUST_BE_LISTENING, nullptr, NO_PATH_LENGTH);
    if (isListeningUnixStream < NO_ERROR_RESULT)
    {
        return SyscallResult<bool>::failure(fromNegatedErrno(isListeningUnixStream));
    }
    return SyscallResult<bool>::success(isListeningUnixStream != NOT_A_MATCH);
}

} // namespace adapter_socket
