#ifndef DOMAIN_REDIRECTION_PROBLEM_H
#define DOMAIN_REDIRECTION_PROBLEM_H

#include <cstdint>
#include <string>

namespace domain {

/* LEARN: the kinds the controller needs to log today. The use case (H-205) may add kinds;
   Unexpected is set only by the controller's defensive catch, never by a use case.
*/
enum class RedirectionProblemKind : uint8_t
{
    KernelRejected,
    TableUnavailable,
    Unexpected
};

struct RedirectionProblem
{
    RedirectionProblemKind kind = RedirectionProblemKind::Unexpected;
    std::string detail;

    bool operator==(const RedirectionProblem&) const = default;
};

} // namespace domain

#endif // DOMAIN_REDIRECTION_PROBLEM_H
