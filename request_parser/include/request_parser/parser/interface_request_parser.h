#ifndef REQUEST_PARSER_INTERFACE_REQUEST_PARSER_H
#define REQUEST_PARSER_INTERFACE_REQUEST_PARSER_H

#include "domain/request/redirection_request.h"
#include "request_parser/parser/parse_problem.h"

#include <expected>
#include <string_view>

namespace request_parser {

/* LEARN: turns one frame's payload into a domain request. A payload that is not a valid request is
   an expected outcome, returned as a ParseProblem value, never thrown. Protobuf stays behind this
   interface: neither the signature nor this header mentions it.
*/
class IRequestParser
{
public:
    virtual ~IRequestParser() = default;

    [[nodiscard]] virtual std::expected<domain::RedirectionRequest, ParseProblem> parse(
        std::string_view payload) const = 0;
};

} // namespace request_parser

#endif // REQUEST_PARSER_INTERFACE_REQUEST_PARSER_H
