#include "domain/logging/logging.h"

#include <cstdint>
#include <gtest/gtest.h>

using domain::Logging;

constexpr int32_t NFLOG_GROUP = 5;
constexpr int32_t OTHER_NFLOG_GROUP = 6;
constexpr int32_t COPY_RANGE = 128;
constexpr int32_t QUEUE_THRESHOLD = 10;

namespace {

Logging fullyPopulated()
{
    return {
        .group = NFLOG_GROUP, .prefix = "DNAT ", .range = COPY_RANGE, .threshold = QUEUE_THRESHOLD};
}

} // namespace

TEST(LoggingTest, DefaultMirrorsTheProto3Defaults)
{
    const Logging logging;

    EXPECT_EQ(logging.group, 0);
    EXPECT_FALSE(logging.prefix.has_value());
    EXPECT_FALSE(logging.range.has_value());
    EXPECT_FALSE(logging.threshold.has_value());
}

TEST(LoggingTest, EqualWhenEveryFieldMatches)
{
    EXPECT_EQ(fullyPopulated(), fullyPopulated());
}

TEST(LoggingTest, DifferentGroupIsNotEqual)
{
    EXPECT_NE(Logging{.group = NFLOG_GROUP}, Logging{.group = OTHER_NFLOG_GROUP});
}

TEST(LoggingTest, UnsetPrefixDiffersFromEmptyPrefix)
{
    EXPECT_NE(Logging{}, Logging{.prefix = ""});
}

TEST(LoggingTest, UnsetRangeDiffersFromZeroRange)
{
    EXPECT_NE(Logging{}, Logging{.range = 0});
}

TEST(LoggingTest, UnsetThresholdDiffersFromZeroThreshold)
{
    EXPECT_NE(Logging{}, Logging{.threshold = 0});
}
