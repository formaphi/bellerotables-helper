#ifndef REQUEST_PARSER_PARSE_PROBLEM_H
#define REQUEST_PARSER_PARSE_PROBLEM_H

#include <cstdint>
#include <string>

namespace request_parser {

/* LEARN: one kind per parsing stage, so the log line says where to look:
   MalformedJson        the payload is not a JSON object (bad syntax, empty, not UTF-8): the agent.
   ContractMismatch     valid JSON, but not the request's shape (unknown or missing field, wrong
                        type): usually the agent and the helper pin different contracts SHAs.
   ConstraintViolation  the shape matches but a buf.validate rule fails (e.g. a negative
                        logging.group, a correlation_id that is not a UUID).
*/
enum class ParseProblemKind : uint8_t
{
    MalformedJson,
    ContractMismatch,
    ConstraintViolation
};

/* LEARN: detail names fields and rules only, never payload content: the controller logs it and
   the raw payload must not reach the journal.
*/
struct ParseProblem
{
    ParseProblemKind kind = ParseProblemKind::MalformedJson;
    std::string detail;

    bool operator==(const ParseProblem&) const = default;
};

} // namespace request_parser

#endif // REQUEST_PARSER_PARSE_PROBLEM_H
