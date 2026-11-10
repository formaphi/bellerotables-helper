#include "request_parser/parser/json_request_parser.h"

#include "domain/logging/logging.h"
#include "domain/request/redirection_request.h"
#include "parameterized_case.h"
#include "request_parser/parser/interface_request_parser.h"
#include "request_parser/parser/parse_problem.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <gtest/gtest.h>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

using domain::Logging;
using domain::RedirectionRequest;
using request_parser::IRequestParser;
using request_parser::JsonRequestParser;
using request_parser::ParseProblem;
using request_parser::ParseProblemKind;

namespace request_parser {

void PrintTo(ParseProblemKind kind, std::ostream* stream)
{
    switch (kind)
    {
        case ParseProblemKind::MalformedJson:
            *stream << "MalformedJson";
            return;
        case ParseProblemKind::ContractMismatch:
            *stream << "ContractMismatch";
            return;
        case ParseProblemKind::ConstraintViolation:
            *stream << "ConstraintViolation";
            return;
    }
}

} // namespace request_parser

namespace {

constexpr const char* CORRELATION_ID = "01932c07-a6b4-7c3e-9f1a-3b5e8d2c4f60";
constexpr int32_t REDIRECTION_ID = 7;
constexpr int32_t NFLOG_GROUP = 5;
constexpr int32_t COPY_RANGE = 128;
constexpr int32_t QUEUE_THRESHOLD = 10;
constexpr int32_t INT32_MAXIMUM = 2147483647;
constexpr int32_t INT32_MINIMUM = -2147483647 - 1;
constexpr std::size_t DEEPER_THAN_ANY_FRAME = 32000;
constexpr std::size_t MAX_FRAME_BYTES = 64 * 1024;
constexpr const char* SECRET = "SECRET";

std::string repeated(const std::string& unit, std::size_t times)
{
    std::string result;
    for (std::size_t index = 0; index < times; ++index)
    {
        result += unit;
    }
    return result;
}

std::string envelope(const std::string& desired)
{
    return std::string(R"({"correlation_id":")") + CORRELATION_ID + R"(","id":7,"desired":)" +
           desired + "}";
}

std::string envelopeWithIds(const std::string& correlationId, const std::string& id)
{
    return R"({"correlation_id":)" + correlationId + R"(,"id":)" + id + R"(,"desired":{}})";
}

RedirectionRequest request(std::optional<Logging> logging)
{
    RedirectionRequest expected{.correlationId = CORRELATION_ID, .id = REDIRECTION_ID};
    expected.desired.logging = std::move(logging);
    return expected;
}

RedirectionRequest requestWithIds(const std::string& correlationId, int32_t id)
{
    return {.correlationId = correlationId, .id = id};
}

} // namespace

struct ValidPayloadCase
{
    std::string payload;
    RedirectionRequest expected;
    const char* name;
};

struct RejectedPayloadCase
{
    std::string payload;
    ParseProblemKind expectedKind;
    const char* name;
};

struct LeakCase
{
    std::string payload;
    const char* name;
};

const std::vector<ValidPayloadCase> VALID_PAYLOADS{
    {envelope("{}"), request(std::nullopt), "LoggingAbsentMeansDisabled"},
    {envelope(R"({"logging":null})"), request(std::nullopt), "LoggingNullMeansDisabled"},
    {envelope(R"({"logging":{}})"), request(Logging{}), "EmptyLoggingEnablesGroupZero"},
    {envelope(R"({"logging":{"group":5,"prefix":"DNAT ","range":128,"threshold":10}})"),
     request(Logging{.group = NFLOG_GROUP,
                     .prefix = "DNAT ",
                     .range = COPY_RANGE,
                     .threshold = QUEUE_THRESHOLD}),
     "EveryLoggingField"},
    {envelope(R"({"logging":{"prefix":""}})"), request(Logging{.prefix = ""}),
     "EmptyPrefixIsKeptAsSet"},
    {envelope(R"({"logging":{"group":"5"}})"), request(Logging{.group = NFLOG_GROUP}),
     "QuotedInt32IsCanonicalProto3Json"},
    {envelope(R"({"logging":{"prefix":")" + repeated("\xC3\xA9", 64) + R"("}})"),
     request(Logging{.prefix = repeated("\xC3\xA9", 64)}), "PrefixOf64MultibyteCharacters"},
    {R"( {"desired":{},"id":7,"correlation_id":")" + std::string(CORRELATION_ID) + R"("} )",
     request(std::nullopt), "KeysInAnyOrderWithWhitespace"},
    {repeated(" ", MAX_FRAME_BYTES - envelope("{}").size()) + envelope("{}"), request(std::nullopt),
     "ValidEnvelopePaddedTo64KiB"},
    {envelopeWithIds(R"("01932C07-A6B4-7C3E-9F1A-3B5E8D2C4F60")", "7"),
     requestWithIds("01932C07-A6B4-7C3E-9F1A-3B5E8D2C4F60", REDIRECTION_ID), "UppercaseUuid"},
    {envelopeWithIds(std::string("\"") + CORRELATION_ID + "\"", "2147483647"),
     requestWithIds(CORRELATION_ID, INT32_MAXIMUM), "MaximumInt32Id"},
    {envelopeWithIds(std::string("\"") + CORRELATION_ID + "\"", "-2147483648"),
     requestWithIds(CORRELATION_ID, INT32_MINIMUM), "MinimumInt32Id"},
};

const std::vector<RejectedPayloadCase> REJECTED_PAYLOADS{
    {"", ParseProblemKind::MalformedJson, "Empty"},
    {"hello", ParseProblemKind::MalformedJson, "NotJson"},
    {R"({"correlation_id":)", ParseProblemKind::MalformedJson, "Truncated"},
    {envelope("{}") + " x", ParseProblemKind::MalformedJson, "TrailingGarbage"},
    {"[]", ParseProblemKind::MalformedJson, "TopLevelArray"},
    {R"("text")", ParseProblemKind::MalformedJson, "TopLevelString"},
    {"{\"correlation_id\":\"\xFF\"}", ParseProblemKind::MalformedJson, "InvalidUtf8"},
    {R"({"correlation_id":"\ud800"})", ParseProblemKind::MalformedJson, "LoneSurrogateEscape"},
    {R"({"id":7,"desired":{}})", ParseProblemKind::ContractMismatch, "MissingCorrelationId"},
    {std::string(R"({"correlation_id":")") + CORRELATION_ID + R"(","desired":{}})",
     ParseProblemKind::ContractMismatch, "MissingId"},
    {std::string(R"({"correlation_id":")") + CORRELATION_ID + R"(","id":7})",
     ParseProblemKind::ContractMismatch, "MissingDesired"},
    {std::string(R"({"correlation_id":")") + CORRELATION_ID +
         R"(","id":7,"desired":{},"status":"QUEUED"})",
     ParseProblemKind::ContractMismatch, "UnknownEnvelopeField"},
    {std::string(R"({"correlationId":")") + CORRELATION_ID + R"(","id":7,"desired":{}})",
     ParseProblemKind::ContractMismatch, "CamelCaseEnvelopeKey"},
    {envelopeWithIds("123", "7"), ParseProblemKind::ContractMismatch, "CorrelationIdNotString"},
    {envelopeWithIds(std::string("\"") + CORRELATION_ID + "\"", R"("7")"),
     ParseProblemKind::ContractMismatch, "IdAsString"},
    {envelopeWithIds(std::string("\"") + CORRELATION_ID + "\"", "7.5"),
     ParseProblemKind::ContractMismatch, "IdFractional"},
    {envelopeWithIds(std::string("\"") + CORRELATION_ID + "\"", "2147483648"),
     ParseProblemKind::ContractMismatch, "IdAboveInt32"},
    {envelopeWithIds(std::string("\"") + CORRELATION_ID + "\"", "-2147483649"),
     ParseProblemKind::ContractMismatch, "IdBelowInt32"},
    {envelope("[]"), ParseProblemKind::ContractMismatch, "DesiredNotObject"},
    {envelope("null"), ParseProblemKind::ContractMismatch, "DesiredNull"},
    {envelope(R"({"id":7})"), ParseProblemKind::ContractMismatch, "IdInsideDesired"},
    {envelope(R"({"logging":{"mark":1}})"), ParseProblemKind::ContractMismatch,
     "UnknownLoggingField"},
    {envelope(R"({"logging":{"group":5.5}})"), ParseProblemKind::ContractMismatch,
     "GroupFractional"},
    {envelope(R"({"logging":{"group":2147483648}})"), ParseProblemKind::ContractMismatch,
     "GroupOutOfInt32Range"},
    {envelope(R"({"logging":{"prefix":7}})"), ParseProblemKind::ContractMismatch,
     "PrefixNotString"},
    {envelope(R"({"logging":{"prefix":)" + repeated("[", DEEPER_THAN_ANY_FRAME) +
              repeated("]", DEEPER_THAN_ANY_FRAME) + "}}"),
     ParseProblemKind::ContractMismatch, "NestedDeeperThanAnyRequest"},
    {envelopeWithIds(R"("")", "7"), ParseProblemKind::ConstraintViolation, "EmptyCorrelationId"},
    {envelopeWithIds("\"" + repeated("a", MAX_FRAME_BYTES) + "\"", "7"),
     ParseProblemKind::ConstraintViolation, "CorrelationIdOf64KiB"},
    {envelopeWithIds(R"("not-a-uuid")", "7"), ParseProblemKind::ConstraintViolation,
     "MalformedCorrelationId"},
    {envelope(R"({"logging":{"group":-1}})"), ParseProblemKind::ConstraintViolation,
     "NegativeGroup"},
    {envelope(R"({"logging":{"range":-1}})"), ParseProblemKind::ConstraintViolation,
     "NegativeRange"},
    {envelope(R"({"logging":{"threshold":0}})"), ParseProblemKind::ConstraintViolation,
     "ZeroThreshold"},
    {envelope(R"({"logging":{"prefix":")" + repeated("x", 65) + R"("}})"),
     ParseProblemKind::ConstraintViolation, "PrefixLongerThan64Characters"},
};

const std::vector<LeakCase> LEAK_PAYLOADS{
    {std::string("{") + SECRET, "MalformedJson"},
    {std::string(R"({"correlation_id":")") + CORRELATION_ID + R"(","id":7,"desired":{},")" +
         SECRET + R"(":1})",
     "UnknownEnvelopeFieldName"},
    {envelope(std::string(R"({")") + SECRET + R"(":1})"), "UnknownDesiredFieldName"},
    {envelope(std::string(R"({"logging":{"group":")") + SECRET + R"("}})"), "BadFieldValue"},
    {envelopeWithIds(std::string("\"") + SECRET + "\"", "7"), "InvalidCorrelationId"},
    {envelope(R"({"logging":{"prefix":")" + repeated(SECRET, 20) + R"("}})"), "TooLongPrefix"},
};

class JsonRequestParserTest
{
protected:
    [[nodiscard]] const IRequestParser& parser() const
    {
        return m_parser;
    }

private:
    JsonRequestParser m_parser;
};

class JsonRequestParserValidTest : public JsonRequestParserTest,
                                   public ::testing::TestWithParam<ValidPayloadCase>
{
};

class JsonRequestParserRejectedTest : public JsonRequestParserTest,
                                      public ::testing::TestWithParam<RejectedPayloadCase>
{
};

class JsonRequestParserLeakTest : public JsonRequestParserTest,
                                  public ::testing::TestWithParam<LeakCase>
{
};

TEST_P(JsonRequestParserValidTest, MapsToTheDomainRequest)
{
    const std::expected<RedirectionRequest, ParseProblem> result =
        parser().parse(GetParam().payload);

    ASSERT_TRUE(result.has_value()) << result.error().detail;
    EXPECT_EQ(*result, GetParam().expected);
}

TEST_P(JsonRequestParserRejectedTest, ReturnsTheProblemKind)
{
    const std::expected<RedirectionRequest, ParseProblem> result =
        parser().parse(GetParam().payload);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, GetParam().expectedKind) << result.error().detail;
    EXPECT_FALSE(result.error().detail.empty());
}

TEST_P(JsonRequestParserLeakTest, DetailNeverEchoesThePayload)
{
    const std::expected<RedirectionRequest, ParseProblem> result =
        parser().parse(GetParam().payload);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail.find(SECRET), std::string::npos) << result.error().detail;
}

TEST(JsonRequestParserDetailTest, ConstraintDetailListsEveryViolationWithItsPath)
{
    const JsonRequestParser parser;
    const std::string payload =
        R"({"correlation_id":"bad","id":7,"desired":{"logging":{"group":-1,"threshold":0}}})";

    const std::expected<RedirectionRequest, ParseProblem> result = parser.parse(payload);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail,
              "correlation_id string.uuid: must be a valid UUID; "
              "desired.logging.group int32.gte: must be greater than or equal to 0; "
              "desired.logging.threshold int32.gte: must be greater than or equal to 1");
}

TEST(JsonRequestParserDetailTest, DeepNestingIsStoppedWhileParsing)
{
    const JsonRequestParser parser;
    const std::string payload =
        envelope(R"({"logging":{"prefix":)" + repeated("[", DEEPER_THAN_ANY_FRAME) +
                 repeated("]", DEEPER_THAN_ANY_FRAME) + "}}");

    const std::expected<RedirectionRequest, ParseProblem> result = parser.parse(payload);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().detail, "payload nests deeper than 16 levels");
}

INSTANTIATE_TEST_SUITE_P(Payload, JsonRequestParserValidTest, ::testing::ValuesIn(VALID_PAYLOADS),
                         caseName<ValidPayloadCase>);

INSTANTIATE_TEST_SUITE_P(Payload, JsonRequestParserRejectedTest,
                         ::testing::ValuesIn(REJECTED_PAYLOADS), caseName<RejectedPayloadCase>);

INSTANTIATE_TEST_SUITE_P(Payload, JsonRequestParserLeakTest, ::testing::ValuesIn(LEAK_PAYLOADS),
                         caseName<LeakCase>);
