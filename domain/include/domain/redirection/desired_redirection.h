#ifndef DOMAIN_DESIRED_REDIRECTION_H
#define DOMAIN_DESIRED_REDIRECTION_H

#include "domain/logging/logging.h"

#include <optional>

namespace domain {

/* LEARN: mirrors bellerophon.ngfw.redirection.DesiredRedirection. Logging absent means
   "disabled" (presence-as-toggle), so it stays optional instead of an enabled flag.
*/
struct DesiredRedirection
{
    std::optional<Logging> logging;

    bool operator==(const DesiredRedirection&) const = default;
};

} // namespace domain

#endif // DOMAIN_DESIRED_REDIRECTION_H
