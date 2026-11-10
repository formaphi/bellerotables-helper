#include "domain/redirection/desired_redirection.h"

#include "domain/logging/logging.h"

#include <cstdint>
#include <gtest/gtest.h>

using domain::DesiredRedirection;
using domain::Logging;

constexpr int32_t NFLOG_GROUP = 5;

TEST(DesiredRedirectionTest, DefaultHasLoggingDisabled)
{
    const DesiredRedirection desired;

    EXPECT_FALSE(desired.logging.has_value());
}

TEST(DesiredRedirectionTest, LoggingEnabledWithDefaultsDiffersFromDisabled)
{
    EXPECT_NE(DesiredRedirection{.logging = Logging{}}, DesiredRedirection{});
}

TEST(DesiredRedirectionTest, EqualityComparesTheNestedLogging)
{
    const DesiredRedirection withGroup{.logging = Logging{.group = NFLOG_GROUP}};

    EXPECT_EQ(withGroup, DesiredRedirection{.logging = Logging{.group = NFLOG_GROUP}});
    EXPECT_NE(withGroup, DesiredRedirection{.logging = Logging{}});
}
