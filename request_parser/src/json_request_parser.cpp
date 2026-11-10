#include "request_parser/parser/json_request_parser.h"

#include "domain/logging/logging.h"
#include "domain/redirection/desired_redirection.h"
#include "domain/request/redirection_request.h"
#include "request_parser/parser/parse_problem.h"

#include <algorithm>
#include <bellerophon/ngfw/common/problem.pb.validate.h>
#include <bellerophon/ngfw/redirection/desired.pb.validate.h>
#include <bellerophon/ngfw/shared/logging.pb.h>
#include <bvalidate/runtime.h>
#include <cstdint>
#include <expected>
#include <google/protobuf/util/json_util.h>
#include <limits>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace request_parser {

namespace {

namespace contract_common = bellerophon::ngfw::common;
namespace contract_redirection = bellerophon::ngfw::redirection;
namespace contract_shared = bellerophon::ngfw::shared;

constexpr std::string_view CORRELATION_ID_KEY = "correlation_id";
constexpr std::string_view ID_KEY = "id";
constexpr std::string_view DESIRED_KEY = "desired";
constexpr std::string_view VIOLATION_SEPARATOR = "; ";

/* LEARN: a request nests 4 levels (envelope, desired, logging, value). The limit is enforced while
   parsing, before anything recursive runs: nlohmann's dump() recurses once per level, and a 64 KiB
   frame of '[' overflows a 2 MiB stack.
*/
constexpr int MAX_NESTING_DEPTH = 16;

struct Envelope
{
    std::string correlationId;
    int32_t id = 0;
    contract_redirection::DesiredRedirection desired;
};

std::unexpected<ParseProblem> problem(ParseProblemKind kind, std::string detail)
{
    return std::unexpected(ParseProblem{.kind = kind, .detail = std::move(detail)});
}

std::expected<nlohmann::json, ParseProblem> readDocument(std::string_view payload)
{
    bool tooDeep = false;
    const nlohmann::json::parser_callback_t depthGuard =
        [&tooDeep](int depth, nlohmann::json::parse_event_t /*event*/, nlohmann::json& /*parsed*/) {
            if (depth > MAX_NESTING_DEPTH)
            {
                tooDeep = true;
                return false;
            }
            return true;
        };

    nlohmann::json document =
        nlohmann::json::parse(payload.begin(), payload.end(), depthGuard, false);
    if (document.is_discarded() || !document.is_object())
    {
        return problem(ParseProblemKind::MalformedJson, "payload is not a JSON object");
    }
    if (tooDeep)
    {
        return problem(
            ParseProblemKind::ContractMismatch,
            "payload nests deeper than " + std::to_string(MAX_NESTING_DEPTH) + " levels");
    }
    return document;
}

std::optional<int32_t> toInt32(const nlohmann::json& value)
{
    constexpr int64_t MINIMUM = std::numeric_limits<int32_t>::min();
    constexpr int64_t MAXIMUM = std::numeric_limits<int32_t>::max();
    if (value.is_number_unsigned())
    {
        const auto number = value.get<uint64_t>();
        return number <= static_cast<uint64_t>(MAXIMUM)
                   ? std::optional<int32_t>(static_cast<int32_t>(number))
                   : std::nullopt;
    }
    if (value.is_number_integer())
    {
        const auto number = value.get<int64_t>();
        return number >= MINIMUM && number <= MAXIMUM
                   ? std::optional<int32_t>(static_cast<int32_t>(number))
                   : std::nullopt;
    }
    return std::nullopt;
}

bool hasOnlyEnvelopeKeys(const nlohmann::json& document)
{
    return std::ranges::all_of(document.items(), [](const auto& item) {
        const std::string& key = item.key();
        return key == CORRELATION_ID_KEY || key == ID_KEY || key == DESIRED_KEY;
    });
}

/* LEARN: details are built from constants and descriptor names only. protobuf's own status
   message is never forwarded: it quotes the offending input ("no such field: '...'"), and the
   detail reaches the journal, where payload content must not.
*/
std::expected<Envelope, ParseProblem> readEnvelope(const nlohmann::json& document)
{
    if (!hasOnlyEnvelopeKeys(document))
    {
        return problem(ParseProblemKind::ContractMismatch,
                       "envelope has a field other than correlation_id, id and desired");
    }

    const auto correlationIdField = document.find(CORRELATION_ID_KEY);
    if (correlationIdField == document.end() || !correlationIdField->is_string())
    {
        return problem(ParseProblemKind::ContractMismatch, "correlation_id must be a string");
    }

    const auto idField = document.find(ID_KEY);
    const std::optional<int32_t> idValue =
        idField == document.end() ? std::nullopt : toInt32(*idField);
    if (!idValue)
    {
        return problem(ParseProblemKind::ContractMismatch, "id must be an int32 number");
    }

    const auto desiredField = document.find(DESIRED_KEY);
    if (desiredField == document.end() || !desiredField->is_object())
    {
        return problem(ParseProblemKind::ContractMismatch, "desired must be a JSON object");
    }

    Envelope envelope;
    envelope.correlationId = correlationIdField->get<std::string>();
    envelope.id = *idValue;
    const bool desiredMatches =
        google::protobuf::util::JsonStringToMessage(desiredField->dump(), &envelope.desired).ok();
    if (!desiredMatches)
    {
        return problem(
            ParseProblemKind::ContractMismatch,
            "desired does not match " +
                std::string(contract_redirection::DesiredRedirection::descriptor()->full_name()));
    }
    return envelope;
}

std::string describe(const bvalidate::Violations& violations)
{
    std::string detail;
    for (const bvalidate::Violation& violation : violations)
    {
        if (!detail.empty())
        {
            detail += VIOLATION_SEPARATOR;
        }
        detail += violation.fieldPath + " " + violation.ruleId + ": " + violation.message;
    }
    return detail;
}

/* LEARN: correlation_id and id are checked as the contract's Accepted message (the API mints them
   there), so the UUID rule comes from the .proto instead of being rewritten here.
*/
std::expected<Envelope, ParseProblem> checkConstraints(Envelope envelope)
{
    contract_common::Accepted identifiers;
    identifiers.set_correlation_id(envelope.correlationId);
    identifiers.set_id(envelope.id);

    bvalidate::Violations violations;
    contract_common::collectViolations(identifiers, std::string(), violations);
    contract_redirection::collectViolations(envelope.desired, std::string(DESIRED_KEY), violations);
    if (!violations.empty())
    {
        return problem(ParseProblemKind::ConstraintViolation, describe(violations));
    }
    return envelope;
}

domain::Logging toDomain(const contract_shared::Logging& logging)
{
    domain::Logging mapped{.group = logging.group()};
    if (logging.has_prefix())
    {
        mapped.prefix = logging.prefix();
    }
    if (logging.has_range())
    {
        mapped.range = logging.range();
    }
    if (logging.has_threshold())
    {
        mapped.threshold = logging.threshold();
    }
    return mapped;
}

domain::RedirectionRequest toRequest(Envelope envelope)
{
    domain::RedirectionRequest request{.correlationId = std::move(envelope.correlationId),
                                       .id = envelope.id};
    if (envelope.desired.has_logging())
    {
        request.desired.logging = toDomain(envelope.desired.logging());
    }
    return request;
}

} // namespace

std::expected<domain::RedirectionRequest, ParseProblem> JsonRequestParser::parse(
    std::string_view payload) const
{
    return readDocument(payload)
        .and_then(readEnvelope)
        .and_then(checkConstraints)
        .transform(toRequest);
}

} // namespace request_parser
