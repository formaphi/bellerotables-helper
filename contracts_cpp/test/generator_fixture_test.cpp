#include "violation_printer.h"

#include <bvalidate/fixture/tree.pb.validate.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using bvalidate::Violation;
using bvalidate::fixture::Leaf;
using bvalidate::fixture::Tree;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::UnorderedElementsAre;

namespace {

const Violation LEAF_COUNT_AT_LEAST_ONE{"count", "int32.gte", "must be greater than or equal to 1"};

Violation leafViolationAt(const std::string& prefix)
{
    Violation violation = LEAF_COUNT_AT_LEAST_ONE;
    violation.fieldPath = prefix + ".count";
    return violation;
}

Leaf validLeaf()
{
    Leaf leaf;
    leaf.set_count(1);
    return leaf;
}

} // namespace

TEST(GeneratorFixtureTest, EmptyTreeIsValid)
{
    EXPECT_THAT(validate(Tree()), IsEmpty());
}

TEST(GeneratorFixtureTest, ImplicitPresenceScalarIsCheckedAtItsZeroValue)
{
    EXPECT_THAT(validate(Leaf()), ElementsAre(LEAF_COUNT_AT_LEAST_ONE));
}

TEST(GeneratorFixtureTest, SetSingularMessageIsRecursedInto)
{
    Tree tree;
    tree.mutable_single();

    EXPECT_THAT(validate(tree), ElementsAre(leafViolationAt("single")));
}

TEST(GeneratorFixtureTest, RepeatedMessagesReportTheirIndex)
{
    Tree tree;
    *tree.add_items() = validLeaf();
    tree.add_items();

    EXPECT_THAT(validate(tree), ElementsAre(leafViolationAt("items[1]")));
}

TEST(GeneratorFixtureTest, MapValuesReportTheirKey)
{
    Tree tree;
    (*tree.mutable_by_name())["ok"] = validLeaf();
    (*tree.mutable_by_name())["bad \"one\""];
    (*tree.mutable_by_id())[-3];

    EXPECT_THAT(validate(tree), UnorderedElementsAre(leafViolationAt(R"(by_name["bad \"one\""])"),
                                                     leafViolationAt("by_id[-3]")));
}

TEST(GeneratorFixtureTest, OneofMembersAreCheckedOnlyWhenSelected)
{
    Tree tree;
    tree.set_token("nope");
    EXPECT_THAT(validate(tree),
                ElementsAre(Violation{"token", "string.uuid", "must be a valid UUID"}));

    tree.mutable_chosen();
    EXPECT_THAT(validate(tree), ElementsAre(leafViolationAt("chosen")));
}

TEST(GeneratorFixtureTest, NestedTypeAndKeywordFieldUseProtoNamesInPaths)
{
    Tree tree;
    tree.mutable_nested()->set_name("four");
    tree.set_class_("abc");

    EXPECT_THAT(
        validate(tree),
        ElementsAre(Violation{"nested.name", "string.max_len", "must be at most 3 characters"},
                    Violation{"class", "string.max_len", "must be at most 2 characters"}));
}

TEST(GeneratorFixtureTest, ContractMessageFromAnotherLibraryIsValidated)
{
    Tree tree;
    tree.mutable_log()->set_group(-1);

    EXPECT_THAT(validate(tree), ElementsAre(Violation{"log.group", "int32.gte",
                                                      "must be greater than or equal to 0"}));
}
