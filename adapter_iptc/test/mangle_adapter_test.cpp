#include <adapter_iptc/adapters/mangle_adapter.h>

#include <adapter_iptc/mocks/mock_iptc_helper.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <linux/netfilter.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_NFLOG.h>
#include <linux/netfilter/xt_mark.h>
#include <linux/netfilter_ipv4/ip_tables.h>

#include <cstddef>
#include <cstdint>
#include <memory>

namespace {

constexpr const char* CHAIN_NAME = "bb_mangle_redir_1";
constexpr uint32_t SAMPLE_MARK = 0x1234;
constexpr uint16_t SAMPLE_NFLOG_GROUP = 5;
constexpr const char* SAMPLE_NFLOG_PREFIX = "bb-log";
constexpr int ACCEPT_VERDICT = -NF_ACCEPT - 1;
constexpr const char* MARK_NAME = "mark";
constexpr const char* CONNTRACK_NAME = "conntrack";
constexpr const char* NFLOG_NAME = "NFLOG";
constexpr const char* EMPTY_TARGET_NAME = "";
constexpr std::size_t EXPECTED_CALL_COUNT = 1;

const xt_entry_match* firstMatch(const ipt_entry& entry)
{
    return reinterpret_cast<const xt_entry_match*>(entry.elems);
}

const xt_entry_match* nextMatch(const xt_entry_match* match)
{
    return reinterpret_cast<const xt_entry_match*>(reinterpret_cast<const unsigned char*>(match) + match->u.user.match_size);
}

const xt_entry_target* targetOf(const ipt_entry& entry)
{
    return reinterpret_cast<const xt_entry_target*>(reinterpret_cast<const unsigned char*>(&entry) + entry.target_offset);
}

const xt_mark_mtinfo1* markPayload(const xt_entry_match* match)
{
    return reinterpret_cast<const xt_mark_mtinfo1*>(match->data);
}

} // namespace

TEST(MangleAdapterTest, CreateChainDelegatesToHelperExactlyOnce)
{
    auto helper = std::make_shared<MockIptcHelper>();
    MangleAdapter adapter(CHAIN_NAME);
    adapter.setHelper(helper);

    EXPECT_CALL(*helper, createChain()).Times(testing::Exactly(EXPECTED_CALL_COUNT));

    adapter.createChain();
}

TEST(MangleAdapterTest, FlushDelegatesToFlushEntriesExactlyOnce)
{
    auto helper = std::make_shared<MockIptcHelper>();
    MangleAdapter adapter(CHAIN_NAME);
    adapter.setHelper(helper);

    EXPECT_CALL(*helper, flushEntries()).Times(testing::Exactly(EXPECTED_CALL_COUNT));

    adapter.flush();
}

TEST(MangleAdapterTest, CommitDelegatesToHelperExactlyOnce)
{
    auto helper = std::make_shared<MockIptcHelper>();
    MangleAdapter adapter(CHAIN_NAME);
    adapter.setHelper(helper);

    EXPECT_CALL(*helper, commit()).Times(testing::Exactly(EXPECTED_CALL_COUNT));

    adapter.commit();
}

TEST(MangleAdapterTest, AddConntrackRuleAppendsMarkThenConntrackMatchWithAcceptTarget)
{
    auto helper = std::make_shared<MockIptcHelper>();
    MangleAdapter adapter(CHAIN_NAME);
    adapter.setHelper(helper);

    EXPECT_CALL(*helper, appendEntry(testing::NotNull()))
        .Times(testing::Exactly(EXPECTED_CALL_COUNT))
        .WillOnce(testing::Invoke([](const ipt_entry* entry) {
            ASSERT_NE(entry, nullptr);
            const xt_entry_match* mark = firstMatch(*entry);
            EXPECT_STREQ(mark->u.user.name, MARK_NAME);
            EXPECT_EQ(markPayload(mark)->mark, SAMPLE_MARK);
            EXPECT_STREQ(nextMatch(mark)->u.user.name, CONNTRACK_NAME);
            const xt_entry_target* target = targetOf(*entry);
            EXPECT_STREQ(target->u.user.name, EMPTY_TARGET_NAME);
            EXPECT_EQ(reinterpret_cast<const xt_standard_target*>(target)->verdict, ACCEPT_VERDICT);
        }));

    adapter.addConntrackRule(MarkParam{SAMPLE_MARK}, ConntrackParam{ConntrackMatch::State::NEW});
}

TEST(MangleAdapterTest, AddNflogRuleAppendsMarkThenNflogTargetWithGroupAndPrefix)
{
    auto helper = std::make_shared<MockIptcHelper>();
    MangleAdapter adapter(CHAIN_NAME);
    adapter.setHelper(helper);

    EXPECT_CALL(*helper, appendEntry(testing::NotNull()))
        .Times(testing::Exactly(EXPECTED_CALL_COUNT))
        .WillOnce(testing::Invoke([](const ipt_entry* entry) {
            ASSERT_NE(entry, nullptr);
            const xt_entry_match* mark = firstMatch(*entry);
            EXPECT_STREQ(mark->u.user.name, MARK_NAME);
            EXPECT_EQ(markPayload(mark)->mark, SAMPLE_MARK);
            const xt_entry_target* target = targetOf(*entry);
            EXPECT_STREQ(target->u.user.name, NFLOG_NAME);
            const auto* info = reinterpret_cast<const xt_nflog_info*>(target->data);
            EXPECT_EQ(info->group, SAMPLE_NFLOG_GROUP);
            EXPECT_STREQ(info->prefix, SAMPLE_NFLOG_PREFIX);
        }));

    adapter.addNflogRule(MarkParam{SAMPLE_MARK}, NflogParam{SAMPLE_NFLOG_PREFIX, SAMPLE_NFLOG_GROUP});
}
