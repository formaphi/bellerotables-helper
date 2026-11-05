#include "adapter_socket/peer_connection/helper_config_dto.h"

#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>

namespace adapter_socket {

constexpr std::string_view AGENT_USER_KEY = "agent_user";
constexpr char KEY_VALUE_SEPARATOR = '=';
constexpr char COMMENT_MARKER = '#';
constexpr std::string_view WHITESPACE_CHARACTERS = " \t\r\n";
constexpr std::size_t START_OF_TEXT = 0;
constexpr std::size_t SEPARATOR_LENGTH = 1;
constexpr std::size_t INCLUSIVE_END = 1;

std::string HelperConfigDto::trim(const std::string& text)
{
    const std::size_t firstKept = text.find_first_not_of(WHITESPACE_CHARACTERS);
    if (firstKept == std::string::npos)
    {
        return {};
    }

    const std::size_t lastKept = text.find_last_not_of(WHITESPACE_CHARACTERS);
    return text.substr(firstKept, lastKept - firstKept + INCLUSIVE_END);
}

HelperConfigDto HelperConfigDto::parse(const std::string& text)
{
    std::istringstream lines(text);
    std::string line;
    while (true)
    {
        const bool hasLine = static_cast<bool>(std::getline(lines, line));
        if (!hasLine)
        {
            break;
        }

        const std::string content = trim(line);
        const bool isBlank = content.empty();
        const bool isComment = !isBlank && content.front() == COMMENT_MARKER;
        const bool isSkippable = isBlank || isComment;
        if (isSkippable)
        {
            continue;
        }

        const std::size_t separatorPosition = content.find(KEY_VALUE_SEPARATOR);
        if (separatorPosition == std::string::npos)
        {
            continue;
        }

        const std::string key = trim(content.substr(START_OF_TEXT, separatorPosition));
        if (key != AGENT_USER_KEY)
        {
            continue;
        }

        HelperConfigDto config;
        config.agentUser = trim(content.substr(separatorPosition + SEPARATOR_LENGTH));
        return config;
    }

    return {};
}

} // namespace adapter_socket
