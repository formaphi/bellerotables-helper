#include "violation_printer.h"

#include <bellerophon/ngfw/common/problem.pb.validate.h>
#include <bellerophon/ngfw/redirection/desired.pb.validate.h>
#include <bellerophon/ngfw/redirection/reported.pb.validate.h>
#include <bellerophon/ngfw/shared/logging.pb.validate.h>
#include <gmock/gmock.h>
#include <google/protobuf/util/json_util.h>
#include <gtest/gtest.h>
#include <string>

using bellerophon::ngfw::common::Accepted;
using bellerophon::ngfw::common::ProblemDetails;
using bellerophon::ngfw::redirection::DesiredRedirection;
using bellerophon::ngfw::redirection::ReportedRedirection;
using bellerophon::ngfw::shared::Logging;
using bvalidate::Violation;
using testing::ElementsAre;
using testing::IsEmpty;

constexpr const char* UUID_V7 = "01932c07-a6b4-7c3e-9f1a-3b5e8d2c4f60";

namespace {

std::string repeated(const std::string& unit, int times)
{
    std::string result;
    for (int index = 0; index < times; ++index)
    {
        result += unit;
    }
    return result;
}

DesiredRedirection parseDesired(const std::string& json)
{
    DesiredRedirection message;
    const auto status = google::protobuf::util::JsonStringToMessage(json, &message);
    EXPECT_TRUE(status.ok()) << status.message();
    return message;
}

} // namespace

TEST(LoggingValidationTest, GroupZeroIsTheMinimumValidLogging)
{
    Logging logging;
    logging.set_group(0);

    EXPECT_THAT(validate(logging), IsEmpty());
}

TEST(LoggingValidationTest, NegativeGroupViolatesGte)
{
    Logging logging;
    logging.set_group(-1);

    EXPECT_THAT(validate(logging),
                ElementsAre(Violation{"group", "int32.gte", "must be greater than or equal to 0"}));
}

TEST(LoggingValidationTest, PrefixOf64CharactersIsAccepted)
{
    Logging logging;
    logging.set_prefix(repeated("x", 64));

    EXPECT_THAT(validate(logging), IsEmpty());
}

TEST(LoggingValidationTest, PrefixOf65CharactersViolatesMaxLen)
{
    Logging logging;
    logging.set_prefix(repeated("x", 65));

    EXPECT_THAT(validate(logging), ElementsAre(Violation{"prefix", "string.max_len",
                                                         "must be at most 64 characters"}));
}

TEST(LoggingValidationTest, MaxLenCountsCodePointsNotBytes)
{
    Logging logging;
    logging.set_prefix(repeated("\xC3\xA9", 64));

    EXPECT_EQ(logging.prefix().size(), 128U);
    EXPECT_THAT(validate(logging), IsEmpty());
}

TEST(LoggingValidationTest, UnsetOptionalThresholdIsNotChecked)
{
    const Logging logging;

    EXPECT_FALSE(logging.has_threshold());
    EXPECT_THAT(validate(logging), IsEmpty());
}

TEST(LoggingValidationTest, ExplicitZeroThresholdViolatesGte)
{
    Logging logging;
    logging.set_threshold(0);

    EXPECT_THAT(validate(logging), ElementsAre(Violation{"threshold", "int32.gte",
                                                         "must be greater than or equal to 1"}));
}

TEST(LoggingValidationTest, NegativeRangeViolatesGte)
{
    Logging logging;
    logging.set_range(-5);

    EXPECT_THAT(validate(logging),
                ElementsAre(Violation{"range", "int32.gte", "must be greater than or equal to 0"}));
}

TEST(DesiredRedirectionValidationTest, AbsentLoggingMeansDisabledAndValid)
{
    EXPECT_THAT(validate(DesiredRedirection()), IsEmpty());
}

TEST(DesiredRedirectionValidationTest, NestedViolationsCarryTheFullPath)
{
    DesiredRedirection desired;
    desired.mutable_logging()->set_group(-1);
    desired.mutable_logging()->set_threshold(0);

    EXPECT_THAT(
        validate(desired),
        ElementsAre(
            Violation{"logging.group", "int32.gte", "must be greater than or equal to 0"},
            Violation{"logging.threshold", "int32.gte", "must be greater than or equal to 1"}));
}

TEST(DesiredRedirectionValidationTest, JsonWireBodyIsParsedThenValidated)
{
    const DesiredRedirection desired =
        parseDesired(R"({"logging": {"group": 5, "prefix": "DNAT ", "threshold": 1}})");

    EXPECT_EQ(desired.logging().group(), 5);
    EXPECT_THAT(validate(desired), IsEmpty());
}

TEST(DesiredRedirectionValidationTest, EmptyLoggingObjectParsesAsGroupZeroAndPasses)
{
    const DesiredRedirection desired = parseDesired(R"({"logging": {}})");

    EXPECT_TRUE(desired.has_logging());
    EXPECT_EQ(desired.logging().group(), 0);
    EXPECT_THAT(validate(desired), IsEmpty());
}

TEST(DesiredRedirectionValidationTest, IdInTheBodyIsRejectedByTheJsonParser)
{
    DesiredRedirection message;
    const auto status = google::protobuf::util::JsonStringToMessage(R"({"id": 7})", &message);

    EXPECT_FALSE(status.ok());
}

TEST(ReportedRedirectionValidationTest, ReusesTheSharedLoggingRules)
{
    ReportedRedirection reported;
    reported.mutable_logging()->set_prefix(repeated("x", 65));

    EXPECT_THAT(validate(reported), ElementsAre(Violation{"logging.prefix", "string.max_len",
                                                          "must be at most 64 characters"}));
}

TEST(AcceptedValidationTest, UuidV7CorrelationIdIsValid)
{
    Accepted accepted;
    accepted.set_correlation_id(UUID_V7);

    EXPECT_THAT(validate(accepted), IsEmpty());
}

TEST(AcceptedValidationTest, EmptyCorrelationIdViolatesUuidEmpty)
{
    EXPECT_THAT(validate(Accepted()),
                ElementsAre(Violation{"correlation_id", "string.uuid_empty",
                                      "value is empty, which is not a valid UUID"}));
}

TEST(AcceptedValidationTest, MalformedCorrelationIdViolatesUuid)
{
    Accepted accepted;
    accepted.set_correlation_id("not-a-uuid");

    EXPECT_THAT(validate(accepted),
                ElementsAre(Violation{"correlation_id", "string.uuid", "must be a valid UUID"}));
}

TEST(AcceptedValidationTest, JsonUsesSnakeCaseKeysWhenPreservingProtoFieldNames)
{
    Accepted accepted;
    accepted.set_correlation_id(UUID_V7);
    accepted.set_status(Accepted::QUEUED);
    google::protobuf::util::JsonPrintOptions options;
    options.preserve_proto_field_names = true;
    std::string json;

    ASSERT_TRUE(google::protobuf::util::MessageToJsonString(accepted, &json, options).ok());
    EXPECT_EQ(json, std::string(R"({"correlation_id":")") + UUID_V7 + R"(","status":"QUEUED"})");
}

TEST(ProblemDetailsValidationTest, UnsetCorrelationIdIsNotChecked)
{
    EXPECT_THAT(validate(ProblemDetails()), IsEmpty());
}

TEST(ProblemDetailsValidationTest, ExplicitlyEmptyCorrelationIdViolatesUuidEmpty)
{
    ProblemDetails problem;
    problem.set_correlation_id("");

    EXPECT_THAT(validate(problem),
                ElementsAre(Violation{"correlation_id", "string.uuid_empty",
                                      "value is empty, which is not a valid UUID"}));
}
