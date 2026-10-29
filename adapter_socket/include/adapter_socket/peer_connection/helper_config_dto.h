#ifndef ADAPTER_SOCKET_HELPER_CONFIG_DTO_H
#define ADAPTER_SOCKET_HELPER_CONFIG_DTO_H

#include <string>

namespace adapter_socket {

struct HelperConfigDto
{
    std::string agentUser;

    [[nodiscard]] static HelperConfigDto parse(const std::string& text);

private:
    [[nodiscard]] static std::string trim(const std::string& text);
};

} // namespace adapter_socket

#endif // ADAPTER_SOCKET_HELPER_CONFIG_DTO_H
