#include <adapter_socket/file_descriptor/readiness.h>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <string>
#include <string_view>

using adapter_socket::Readiness;

constexpr std::string_view READABLE_NAME = "read";
constexpr std::string_view WRITABLE_NAME = "write";
constexpr std::string_view ACCEPTABLE_NAME = "accept";

TEST(ReadinessTest, ReadableSerializesToRead)
{
    const nlohmann::json name = Readiness::Readable;

    EXPECT_EQ(name.get<std::string>(), READABLE_NAME);
}

TEST(ReadinessTest, WritableSerializesToWrite)
{
    const nlohmann::json name = Readiness::Writable;

    EXPECT_EQ(name.get<std::string>(), WRITABLE_NAME);
}

TEST(ReadinessTest, AcceptableSerializesToAccept)
{
    const nlohmann::json name = Readiness::Acceptable;

    EXPECT_EQ(name.get<std::string>(), ACCEPTABLE_NAME);
}
