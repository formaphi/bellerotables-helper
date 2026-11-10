#ifndef DOMAIN_REDIRECTION_REQUEST_H
#define DOMAIN_REDIRECTION_REQUEST_H

#include "domain/redirection/desired_redirection.h"

#include <cstdint>
#include <string>

namespace domain {

/* LEARN: one request from the agent, the envelope {correlation_id, id, desired}. correlationId and
   id mirror bellerophon.ngfw.common.Accepted; the envelope itself has no contract message yet.
*/
struct RedirectionRequest
{
    std::string correlationId;
    int32_t id = 0;
    DesiredRedirection desired;

    bool operator==(const RedirectionRequest&) const = default;
};

} // namespace domain

#endif // DOMAIN_REDIRECTION_REQUEST_H
