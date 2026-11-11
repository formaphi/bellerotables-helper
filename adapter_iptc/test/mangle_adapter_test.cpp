#include "adapter_iptc/adapters/mangle_adapter.h"

#include "adapter_iptc/builder/rule_builder.h"
#include "adapter_iptc/dtos/mark.h"
#include "adapter_iptc/dtos/nflog.h"
#include "adapter_iptc/exceptions/iptc_exception.h"
#include "adapter_iptc/ipt_entry_ptr.h"
#include "adapter_iptc/matchs/conntrack_match.h"
#include "adapter_iptc/matchs/mark_match.h"
#include "adapter_iptc/mocks/mock_iptc_helper.h"
#include "adapter_iptc/targets/nflog_target.h"
#include "adapter_iptc/targets/standard_target.h"
#include "parameterized_case.h"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <linux/netfilter.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_mark.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace {

constexpr const char* CHAIN_NAME = "bb_mangle_redir_1";
constexpr uint32_t SAMPLE_MARK = 0x1234;
constexpr uint32_t OTHER_MARK = 0x5678;
constexpr uint16_t SAMPLE_NFLOG_GROUP = 5;
constexpr uint16_t OTHER_NFLOG_GROUP = 6;
constexpr const char* SAMPLE_NFLOG_PREFIX = "bb-log";
constexpr uint32_t SAMPLE_NFLOG_RANGE = 128;
constexpr uint16_t SAMPLE_NFLOG_THRESHOLD = 10;
constexpr int ACCEPT_VERDICT = -NF_ACCEPT - 1;
constexpr const char* MARK_NAME = "mark";
constexpr const char* CONNTRACK_NAME = "conntrack";
constexpr const char* EMPTY_TARGET_NAME = "";
constexpr std::size_t EXPECTED_CALL_COUNT = 1;

NflogParam fullNflog()
{
    return NflogParam{.prefix = SAMPLE_NFLOG_PREFIX,
                      .group = SAMPLE_NFLOG_GROUP,
                      .range = SAMPLE_NFLOG_RANGE,
                      .threshold = SAMPLE_NFLOG_THRESHOLD};
}

NflogParam minimalNflog()
{
    return NflogParam{.prefix = "", .group = OTHER_NFLOG_GROUP};
}

const xt_entry_match* firstMatch(const ipt_entry& entry)
{
    return reinterpret_cast<const xt_entry_match*>(entry.elems);
}

const xt_entry_match* nextMatch(const xt_entry_match* match)
{
    const auto* matchStart = reinterpret_cast<const unsigned char*>(match);
    const unsigned char* nextStart = matchStart + match->u.user.match_size;
    return reinterpret_cast<const xt_entry_match*>(nextStart);
}

const xt_entry_target* targetOf(const ipt_entry& entry)
{
    const auto* entryStart = reinterpret_cast<const unsigned char*>(&entry);
    const unsigned char* targetStart = entryStart + entry.target_offset;
    return reinterpret_cast<const xt_entry_target*>(targetStart);
}

const xt_mark_mtinfo1* markPayload(const xt_entry_match* match)
{
    return reinterpret_cast<const xt_mark_mtinfo1*>(match->data);
}

/* LEARN: appendEntry() only borrows the rule buffer, which addNflogRule() frees on return, so a
   test that plays the kernel keeps its own copy, as IptcHelper::listEntries() copies each rule
   out of the libiptc snapshot.
*/
IptEntryPtr copyOf(const ipt_entry& entry)
{
    void* buffer = std::calloc(1, entry.next_offset);
    auto* rule = static_cast<ipt_entry*>(buffer);
    IptEntryPtr copy(rule, &std::free);
    std::memcpy(copy.get(), &entry, entry.next_offset);
    return copy;
}

IptEntryPtr nflogEntry(uint32_t mark, const NflogParam& nflog)
{
    return RuleBuilder()
        .setMatch(std::make_shared<MarkMatch>(mark))
        .setTarget(std::make_shared<NflogTarget>(nflog))
        .build();
}

IptEntryPtr unmarkedNflogEntry(const NflogParam& nflog)
{
    return RuleBuilder().setTarget(std::make_shared<NflogTarget>(nflog)).build();
}

IptEntryPtr conntrackEntry()
{
    return RuleBuilder()
        .setMatch(std::make_shared<MarkMatch>(SAMPLE_MARK))
        .setMatch(std::make_shared<ConntrackMatch>(ConntrackMatch::State::NEW))
        .setTarget(std::make_shared<StandardTarget>(StandardTarget::Verdict::ACCEPT))
        .build();
}

} // namespace

/* LEARN: every test drives one MangleAdapter whose libiptc helper is a mock, so it checks what
   the adapter asks of libiptc without a kernel. The mock verifies its expectations when the
   fixture is destroyed.
*/
class MangleAdapterTest : public ::testing::Test
{
protected:
    MangleAdapterTest()
        : m_adapter(CHAIN_NAME)
    {
        m_adapter.setHelper(m_helper);
    }

    std::shared_ptr<MockIptcHelper> m_helper = std::make_shared<MockIptcHelper>();
    MangleAdapter m_adapter;
};

TEST_F(MangleAdapterTest, CreateChainDelegatesToHelperExactlyOnce)
{
    EXPECT_CALL(*m_helper, chainExists()).WillOnce(testing::Return(false));
    EXPECT_CALL(*m_helper, createChain()).Times(testing::Exactly(EXPECTED_CALL_COUNT));

    m_adapter.createChain();
}

TEST_F(MangleAdapterTest, CreateChainSkipsAChainThatAlreadyExists)
{
    EXPECT_CALL(*m_helper, chainExists()).WillOnce(testing::Return(true));
    EXPECT_CALL(*m_helper, createChain()).Times(0);

    m_adapter.createChain();
}

TEST_F(MangleAdapterTest, FlushDelegatesToFlushEntriesExactlyOnce)
{
    EXPECT_CALL(*m_helper, flushEntries()).Times(testing::Exactly(EXPECTED_CALL_COUNT));

    m_adapter.flush();
}

TEST_F(MangleAdapterTest, CommitDelegatesToHelperExactlyOnce)
{
    EXPECT_CALL(*m_helper, commit()).Times(testing::Exactly(EXPECTED_CALL_COUNT));

    m_adapter.commit();
}

TEST_F(MangleAdapterTest, AddConntrackRuleAppendsMarkThenConntrackMatchWithAcceptTarget)
{
    EXPECT_CALL(*m_helper, appendEntry(testing::NotNull()))
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

    m_adapter.addConntrackRule(MarkParam{SAMPLE_MARK}, ConntrackParam{ConntrackMatch::State::NEW});
}

TEST_F(MangleAdapterTest, ReadNflogRulesIsEmptyWithoutListingWhenTheChainIsMissing)
{
    EXPECT_CALL(*m_helper, chainExists()).WillOnce(testing::Return(false));
    EXPECT_CALL(*m_helper, listEntries()).Times(0);

    EXPECT_TRUE(m_adapter.readNflogRules().empty());
}

TEST_F(MangleAdapterTest, WhatAddNflogRuleAppendsReadsBackUnchanged)
{
    std::vector<IptEntryPtr> kernel;

    EXPECT_CALL(*m_helper, appendEntry(testing::NotNull()))
        .WillOnce(testing::Invoke(
            [&kernel](const ipt_entry* entry) { kernel.push_back(copyOf(*entry)); }));
    EXPECT_CALL(*m_helper, chainExists()).WillOnce(testing::Return(true));
    EXPECT_CALL(*m_helper, listEntries()).WillOnce(testing::Invoke([&kernel] {
        return std::move(kernel);
    }));

    m_adapter.addNflogRule(MarkParam{SAMPLE_MARK}, fullNflog());

    EXPECT_EQ(
        m_adapter.readNflogRules(),
        (std::vector<NflogRuleParam>{{.mark = MarkParam{SAMPLE_MARK}, .nflog = fullNflog()}}));
}

TEST_F(MangleAdapterTest, ReadNflogRulesLetsTheHelperErrorThrough)
{
    EXPECT_CALL(*m_helper, chainExists())
        .WillOnce(testing::Throw(IptcException("iptc_init(mangle): Table does not exist", ENOENT)));

    EXPECT_THROW(static_cast<void>(m_adapter.readNflogRules()), IptcException);
}

/* LEARN: addNflogRule() appends one rule: the mark match, then the NFLOG target carrying every
   knob of the NflogParam. The appended buffer is decoded with the readers (tested on their own
   in match_test.cpp and nflog_target_test.cpp), so every field is compared at once.
*/
struct AddNflogCase
{
    NflogParam nflog;
    const char* name;
};

const std::vector<AddNflogCase> ADD_NFLOG_CASES{
    {NflogParam{.prefix = SAMPLE_NFLOG_PREFIX, .group = SAMPLE_NFLOG_GROUP}, "GroupAndPrefix"},
    {fullNflog(), "EveryKnob"},
};

class MangleAdapterAddNflogTest : public MangleAdapterTest,
                                  public ::testing::WithParamInterface<AddNflogCase>
{
};

TEST_P(MangleAdapterAddNflogTest, AppendsMarkMatchThenNflogTarget)
{
    const NflogParam expected = GetParam().nflog;

    EXPECT_CALL(*m_helper, appendEntry(testing::NotNull()))
        .Times(testing::Exactly(EXPECTED_CALL_COUNT))
        .WillOnce(testing::Invoke([&expected](const ipt_entry* entry) {
            EXPECT_STREQ(firstMatch(*entry)->u.user.name, MARK_NAME);
            EXPECT_EQ(MarkMatch::read(*entry), MarkParam{SAMPLE_MARK});
            EXPECT_EQ(NflogTarget::read(*entry), expected);
        }));

    m_adapter.addNflogRule(MarkParam{SAMPLE_MARK}, GetParam().nflog);
}

INSTANTIATE_TEST_SUITE_P(MangleAdapter, MangleAdapterAddNflogTest,
                         ::testing::ValuesIn(ADD_NFLOG_CASES), caseName<AddNflogCase>);

/* LEARN: what the chain holds (the entries listEntries() hands over) and what readNflogRules()
   must report: only NFLOG rules, in rule order, each with the mark it matches.
*/
struct ReadNflogCase
{
    std::vector<IptEntryPtr> (*kernel)();
    std::vector<NflogRuleParam> expected;
    const char* name;
};

const std::vector<ReadNflogCase> READ_NFLOG_CASES{
    {[]() { return std::vector<IptEntryPtr>(); }, {}, "EmptyChain"},
    {[]() {
         std::vector<IptEntryPtr> entries;
         entries.push_back(conntrackEntry());
         entries.push_back(nflogEntry(SAMPLE_MARK, fullNflog()));
         entries.push_back(conntrackEntry());
         entries.push_back(nflogEntry(OTHER_MARK, minimalNflog()));
         return entries;
     },
     {{.mark = MarkParam{SAMPLE_MARK}, .nflog = fullNflog()},
      {.mark = MarkParam{OTHER_MARK}, .nflog = minimalNflog()}},
     "OnlyNflogRulesWithTheirMarkInRuleOrder"},
    {[]() {
         std::vector<IptEntryPtr> entries;
         entries.push_back(unmarkedNflogEntry(fullNflog()));
         return entries;
     },
     {{.mark = std::nullopt, .nflog = fullNflog()}},
     "NflogRuleWithoutAMarkMatchReadsWithoutMark"},
};

class MangleAdapterReadNflogTest : public MangleAdapterTest,
                                   public ::testing::WithParamInterface<ReadNflogCase>
{
};

TEST_P(MangleAdapterReadNflogTest, ReportsTheNflogRulesOfTheChain)
{
    EXPECT_CALL(*m_helper, chainExists()).WillOnce(testing::Return(true));
    EXPECT_CALL(*m_helper, listEntries())
        .WillOnce(testing::Return(testing::ByMove(GetParam().kernel())));

    EXPECT_EQ(m_adapter.readNflogRules(), GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(MangleAdapter, MangleAdapterReadNflogTest,
                         ::testing::ValuesIn(READ_NFLOG_CASES), caseName<ReadNflogCase>);
