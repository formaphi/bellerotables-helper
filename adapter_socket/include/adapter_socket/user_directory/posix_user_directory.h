#ifndef ADAPTER_SOCKET_POSIX_USER_DIRECTORY_H
#define ADAPTER_SOCKET_POSIX_USER_DIRECTORY_H

#include "adapter_socket/user_directory/interface_user_directory.h"

namespace adapter_socket {

class PosixUserDirectory final : public IUserDirectory
{
public:
    PosixUserDirectory() = default;
    ~PosixUserDirectory() override = default;

    PosixUserDirectory(const PosixUserDirectory&) = delete;
    PosixUserDirectory& operator=(const PosixUserDirectory&) = delete;
    PosixUserDirectory(PosixUserDirectory&&) = delete;
    PosixUserDirectory& operator=(PosixUserDirectory&&) = delete;

    [[nodiscard]] std::optional<uint32_t> findUserId(const std::string& userName) const override;
};

} // namespace adapter_socket

#endif /* ADAPTER_SOCKET_POSIX_USER_DIRECTORY_H */
