#ifndef DOMAIN_INTERFACE_REDIRECTION_USE_CASE_H
#define DOMAIN_INTERFACE_REDIRECTION_USE_CASE_H

#include "domain/request/redirection_request.h"
#include "domain/use_case/redirection_problem.h"

#include <expected>

namespace domain {

/* LEARN: expected failures are values, not exceptions: an implementation converts its adapters'
   exceptions into a RedirectionProblem. E3 replaces void with the observed kernel state.
*/
class IRedirectionUseCase
{
public:
    virtual ~IRedirectionUseCase() = default;

    [[nodiscard]] virtual std::expected<void, RedirectionProblem> execute(
        const RedirectionRequest& request) = 0;
};

} // namespace domain

#endif // DOMAIN_INTERFACE_REDIRECTION_USE_CASE_H
