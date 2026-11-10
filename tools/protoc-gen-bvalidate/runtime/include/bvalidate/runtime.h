#ifndef BVALIDATE_RUNTIME_H
#define BVALIDATE_RUNTIME_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace bvalidate {

/* LEARN: mirrors buf.validate.Violation as protovalidate reports it: the same rule ids and
   messages, and the field path rendered the way protovalidate prints a FieldPath (a.b[0].c["key"]).
*/
struct Violation
{
    std::string fieldPath;
    std::string ruleId;
    std::string message;

    bool operator==(const Violation&) const = default;
};

using Violations = std::vector<Violation>;

[[nodiscard]] std::string fieldPath(std::string_view parent, std::string_view fieldName);
[[nodiscard]] std::string indexPath(std::string_view parent, std::string_view fieldName,
                                    std::size_t index);
[[nodiscard]] std::string keyPath(std::string_view parent, std::string_view fieldName,
                                  std::string_view key);
[[nodiscard]] std::string keyPath(std::string_view parent, std::string_view fieldName, int64_t key);
[[nodiscard]] std::string keyPath(std::string_view parent, std::string_view fieldName,
                                  uint64_t key);
[[nodiscard]] std::string keyPath(std::string_view parent, std::string_view fieldName, bool key);

// LEARN: protovalidate counts string length in Unicode code points, not bytes.
[[nodiscard]] std::size_t codePointCount(std::string_view utf8);
[[nodiscard]] bool isUuid(std::string_view value);

void checkInt32Gte(int32_t value, int32_t minimum, std::string path, Violations& violations);
void checkStringMaxLen(std::string_view value, uint64_t maximum, std::string path,
                       Violations& violations);
void checkStringUuid(std::string_view value, std::string path, Violations& violations);

} // namespace bvalidate

#endif // BVALIDATE_RUNTIME_H
