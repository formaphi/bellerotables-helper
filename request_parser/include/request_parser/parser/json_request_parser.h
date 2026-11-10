#ifndef REQUEST_PARSER_JSON_REQUEST_PARSER_H
#define REQUEST_PARSER_JSON_REQUEST_PARSER_H

#include "request_parser/parser/interface_request_parser.h"

#include <expected>
#include <string_view>

namespace request_parser {

/* LEARN: the payload is the envelope {"correlation_id", "id", "desired"}. The envelope has no
   contract message yet, so nlohmann reads it; "desired" is DesiredRedirection in canonical proto3
   JSON and goes through protobuf's JsonStringToMessage. Every buf.validate rule then runs through
   the generated validators (correlation_id and id as bellerophon.ngfw.common.Accepted) before
   the result is mapped to domain types. nlohmann and protobuf stay in the .cpp.
*/
class JsonRequestParser final : public IRequestParser
{
public:
    JsonRequestParser() = default;
    ~JsonRequestParser() override = default;

    JsonRequestParser(const JsonRequestParser&) = delete;
    JsonRequestParser& operator=(const JsonRequestParser&) = delete;
    JsonRequestParser(JsonRequestParser&&) = delete;
    JsonRequestParser& operator=(JsonRequestParser&&) = delete;

    [[nodiscard]] std::expected<domain::RedirectionRequest, ParseProblem> parse(
        std::string_view payload) const override;
};

} // namespace request_parser

#endif // REQUEST_PARSER_JSON_REQUEST_PARSER_H
