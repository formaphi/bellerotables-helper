#include "domain/request/redirection_request.h"

#include "domain/logging/logging.h"
#include "domain/redirection/desired_redirection.h"

#include <cstdint>
#include <gtest/gtest.h>

using domain::DesiredRedirection;
using domain::Logging;
using domain::RedirectionRequest;

constexpr const char* CORRELATION_ID = "01932c07-a6b4-7c3e-9f1a-3b5e8d2c4f60";
constexpr const char* OTHER_CORRELATION_ID = "01932c07-a6b4-7c3e-9f1a-3b5e8d2c4f61";
constexpr int32_t REDIRECTION_ID = 42;
constexpr int32_t OTHER_REDIRECTION_ID = 43;
constexpr int32_t NFLOG_GROUP = 5;

namespace {

RedirectionRequest sampleRequest()
{
    return {.correlationId = CORRELATION_ID,
            .id = REDIRECTION_ID,
            .desired = DesiredRedirection{.logging = Logging{.group = NFLOG_GROUP}}};
}

} // namespace

TEST(RedirectionRequestTest, DefaultIsEmptyWithLoggingDisabled)
{
    const RedirectionRequest request;

    EXPECT_TRUE(request.correlationId.empty());
    EXPECT_EQ(request.id, 0);
    EXPECT_FALSE(request.desired.logging.has_value());
}

TEST(RedirectionRequestTest, EqualWhenEveryFieldMatches)
{
    EXPECT_EQ(sampleRequest(), sampleRequest());
}

TEST(RedirectionRequestTest, DifferentCorrelationIdIsNotEqual)
{
    RedirectionRequest other = sampleRequest();
    other.correlationId = OTHER_CORRELATION_ID;

    EXPECT_NE(sampleRequest(), other);
}

TEST(RedirectionRequestTest, DifferentIdIsNotEqual)
{
    RedirectionRequest other = sampleRequest();
    other.id = OTHER_REDIRECTION_ID;

    EXPECT_NE(sampleRequest(), other);
}

TEST(RedirectionRequestTest, DifferentDesiredIsNotEqual)
{
    RedirectionRequest other = sampleRequest();
    other.desired.logging.reset();

    EXPECT_NE(sampleRequest(), other);
}
