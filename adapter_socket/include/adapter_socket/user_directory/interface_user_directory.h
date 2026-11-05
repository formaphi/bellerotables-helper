#if !defined(INTERFACE_USER_DIRECTORY_H)
#define INTERFACE_USER_DIRECTORY_H

#include <cstdint>
#include <optional>
#include <string>

namespace adapter_socket {

class IUserDirectory
{
public:
    virtual ~IUserDirectory() = default;

    [[nodiscard]] virtual std::optional<uint32_t> findUserId(const std::string& userName) const = 0;
};

} // namespace adapter_socket

#endif // INTERFACE_USER_DIRECTORY_H
