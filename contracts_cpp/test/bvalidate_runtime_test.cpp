#include <bvalidate/runtime.h>
#include <gtest/gtest.h>

TEST(BvalidateRuntimeTest, CodePointCountIgnoresUtf8ContinuationBytes)
{
    EXPECT_EQ(bvalidate::codePointCount(""), 0U);
    EXPECT_EQ(bvalidate::codePointCount("abc"), 3U);
    EXPECT_EQ(bvalidate::codePointCount("\xC3\xA9"), 1U);
    EXPECT_EQ(bvalidate::codePointCount("\xF0\x9F\x94\xA5!"), 2U);
}

TEST(BvalidateRuntimeTest, UuidAcceptsBothCasesAndRejectsWrongShapes)
{
    EXPECT_TRUE(bvalidate::isUuid("01932c07-a6b4-7c3e-9f1a-3b5e8d2c4f60"));
    EXPECT_TRUE(bvalidate::isUuid("01932C07-A6B4-7C3E-9F1A-3B5E8D2C4F60"));
    EXPECT_FALSE(bvalidate::isUuid("01932c07a6b47c3e9f1a3b5e8d2c4f60"));
    EXPECT_FALSE(bvalidate::isUuid("{01932c07-a6b4-7c3e-9f1a-3b5e8d2c4f60}"));
    EXPECT_FALSE(bvalidate::isUuid("01932c07-a6b4-7c3e-9f1a-3b5e8d2c4f6g"));
    EXPECT_FALSE(bvalidate::isUuid(" 01932c07-a6b4-7c3e-9f1a-3b5e8d2c4f6"));
}

TEST(BvalidateRuntimeTest, PathsRenderLikeProtovalidate)
{
    EXPECT_EQ(bvalidate::fieldPath("", "a"), "a");
    EXPECT_EQ(bvalidate::fieldPath("a", "b"), "a.b");
    EXPECT_EQ(bvalidate::indexPath("a", "b", 2), "a.b[2]");
    EXPECT_EQ(bvalidate::keyPath("", "m", std::string_view("k")), R"(m["k"])");
    EXPECT_EQ(bvalidate::keyPath("", "m", int64_t{-1}), "m[-1]");
    EXPECT_EQ(bvalidate::keyPath("", "m", true), "m[true]");
}
