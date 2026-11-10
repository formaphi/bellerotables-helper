#ifndef DOMAIN_LOGGING_H
#define DOMAIN_LOGGING_H

#include <cstdint>
#include <optional>
#include <string>

namespace domain {

/* LEARN: mirrors bellerophon.ngfw.shared.Logging (NFLOG knobs); presence of the optional fields
   is kept, so "not set" and "set to the zero value" stay distinct. The protovalidate rules are not
   repeated here: the parser validates before it maps.
*/
struct Logging
{
    int32_t group = 0;
    std::optional<std::string> prefix;
    std::optional<int32_t> range;
    std::optional<int32_t> threshold;

    bool operator==(const Logging&) const = default;
};

} // namespace domain

#endif // DOMAIN_LOGGING_H
