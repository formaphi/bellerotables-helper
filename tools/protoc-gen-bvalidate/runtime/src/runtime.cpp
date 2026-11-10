#include "bvalidate/runtime.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <format>
#include <string>
#include <utility>

namespace bvalidate {

namespace {

constexpr std::size_t UUID_LENGTH = 36;
constexpr std::array<std::size_t, 4> UUID_DASH_POSITIONS{8, 13, 18, 23};
constexpr unsigned char UTF8_CONTINUATION_MASK = 0xC0;
constexpr unsigned char UTF8_CONTINUATION_BITS = 0x80;

std::string quoted(std::string_view key)
{
    std::string result = "\"";
    for (const char character : key)
    {
        if (character == '"' || character == '\\')
        {
            result += '\\';
        }
        result += character;
    }
    result += '"';
    return result;
}

bool isDashPosition(std::size_t position)
{
    return std::ranges::any_of(UUID_DASH_POSITIONS,
                               [position](std::size_t dash) { return dash == position; });
}

} // namespace

std::string fieldPath(std::string_view parent, std::string_view fieldName)
{
    if (parent.empty())
    {
        return std::string(fieldName);
    }
    return std::format("{}.{}", parent, fieldName);
}

std::string indexPath(std::string_view parent, std::string_view fieldName, std::size_t index)
{
    return std::format("{}[{}]", fieldPath(parent, fieldName), index);
}

std::string keyPath(std::string_view parent, std::string_view fieldName, std::string_view key)
{
    return std::format("{}[{}]", fieldPath(parent, fieldName), quoted(key));
}

std::string keyPath(std::string_view parent, std::string_view fieldName, int64_t key)
{
    return std::format("{}[{}]", fieldPath(parent, fieldName), key);
}

std::string keyPath(std::string_view parent, std::string_view fieldName, uint64_t key)
{
    return std::format("{}[{}]", fieldPath(parent, fieldName), key);
}

std::string keyPath(std::string_view parent, std::string_view fieldName, bool key)
{
    return std::format("{}[{}]", fieldPath(parent, fieldName), key ? "true" : "false");
}

std::size_t codePointCount(std::string_view utf8)
{
    std::size_t count = 0;
    for (const char character : utf8)
    {
        const auto byte = static_cast<unsigned char>(character);
        if ((byte & UTF8_CONTINUATION_MASK) != UTF8_CONTINUATION_BITS)
        {
            ++count;
        }
    }
    return count;
}

bool isUuid(std::string_view value)
{
    if (value.size() != UUID_LENGTH)
    {
        return false;
    }
    for (std::size_t position = 0; position < value.size(); ++position)
    {
        const char character = value[position];
        const bool valid = isDashPosition(position)
                               ? character == '-'
                               : std::isxdigit(static_cast<unsigned char>(character)) != 0;
        if (!valid)
        {
            return false;
        }
    }
    return true;
}

void checkInt32Gte(int32_t value, int32_t minimum, std::string path, Violations& violations)
{
    if (value < minimum)
    {
        violations.push_back({std::move(path), "int32.gte",
                              std::format("must be greater than or equal to {}", minimum)});
    }
}

void checkStringMaxLen(std::string_view value, uint64_t maximum, std::string path,
                       Violations& violations)
{
    if (codePointCount(value) > maximum)
    {
        violations.push_back({std::move(path), "string.max_len",
                              std::format("must be at most {} characters", maximum)});
    }
}

void checkStringUuid(std::string_view value, std::string path, Violations& violations)
{
    if (value.empty())
    {
        violations.push_back(
            {std::move(path), "string.uuid_empty", "value is empty, which is not a valid UUID"});
        return;
    }
    if (!isUuid(value))
    {
        violations.push_back({std::move(path), "string.uuid", "must be a valid UUID"});
    }
}

} // namespace bvalidate
