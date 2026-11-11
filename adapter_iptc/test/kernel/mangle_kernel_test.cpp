#include "adapter_iptc/adapters/iptc_helper.h"
#include "adapter_iptc/adapters/mangle_adapter.h"
#include "adapter_iptc/builder/rule_builder.h"
#include "adapter_iptc/dtos/conntrack.h"
#include "adapter_iptc/dtos/mark.h"
#include "adapter_iptc/dtos/nflog.h"
#include "adapter_iptc/exceptions/iptc_exception.h"
#include "adapter_iptc/ipt_entry_ptr.h"
#include "adapter_iptc/matchs/conntrack_match.h"
#include "adapter_iptc/targets/extension_target_template.h"
#include "adapter_iptc/targets/nflog_target.h"
#include "network_namespace.h"
#include "parameterized_case.h"

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr const char* MANGLE_TABLE = "mangle";
constexpr uint32_t SAMPLE_MARK = 0x1234;
constexpr uint16_t SAMPLE_NFLOG_GROUP = 5;
constexpr uint16_t OTHER_NFLOG_GROUP = 6;
constexpr const char* SAMPLE_NFLOG_PREFIX = "DNAT ";
constexpr uint32_t SAMPLE_NFLOG_RANGE = 128;
constexpr uint16_t SAMPLE_NFLOG_THRESHOLD = 10;
constexpr const char* TWO_BYTE_CHARACTER = "\xC3\xA9";
constexpr std::size_t TWO_BYTE_CHARACTERS_THAT_FIT = 31;
constexpr std::size_t CONTRACT_PREFIX_CHARACTERS = 64;

NflogParam fullNflog()
{
    return NflogParam{.prefix = SAMPLE_NFLOG_PREFIX,
                      .group = SAMPLE_NFLOG_GROUP,
                      .range = SAMPLE_NFLOG_RANGE,
                      .threshold = SAMPLE_NFLOG_THRESHOLD};
}

NflogRuleParam marked(const NflogParam& nflog)
{
    return NflogRuleParam{.mark = MarkParam{SAMPLE_MARK}, .nflog = nflog};
}

std::string repeated(const std::string& unit, std::size_t times)
{
    std::string result;
    for (std::size_t index = 0; index < times; ++index)
    {
        result += unit;
    }
    return result;
}

NflogParam changedNflog()
{
    return NflogParam{.prefix = "changed ", .group = OTHER_NFLOG_GROUP};
}

/* LEARN: the longest prefix the contract allows in 2-byte characters (128 bytes); the kernel
   keeps 63 bytes, cut before the character it would split.
*/
NflogParam multiByteNflog()
{
    return NflogParam{.prefix = repeated(TWO_BYTE_CHARACTER, CONTRACT_PREFIX_CHARACTERS),
                      .group = SAMPLE_NFLOG_GROUP};
}

NflogParam withPrefix(NflogParam nflog, std::string prefix)
{
    nflog.prefix = std::move(prefix);
    return nflog;
}

/* LEARN: one activation of the helper: a new adapter (new libiptc snapshot) per call, as each
   systemd activation is a new process.
*/
void applyLogging(const std::string& chain, const std::vector<NflogParam>& rules)
{
    MangleAdapter adapter(chain);
    adapter.createChain();
    adapter.flush();
    for (const NflogParam& nflog : rules)
    {
        adapter.addNflogRule(MarkParam{SAMPLE_MARK}, nflog);
    }
    adapter.commit();
}

std::vector<NflogRuleParam> readFromKernel(const std::string& chain)
{
    MangleAdapter reader(chain);
    return reader.readNflogRules();
}

class UnknownTarget final : public ExtensionTargetTemplate
{
protected:
    const char* name() const override
    {
        return "BBNOSUCHTARGET";
    }

    std::size_t dataSize() const override
    {
        return sizeof(uint32_t);
    }

    void fillData(unsigned char* /*data*/) const override {}
};

/* LEARN: queues a rule whose target does not exist: libiptc accepts it, the kernel rejects the
   whole table at commit.
*/
void appendUnknownTarget(IptcHelper& helper)
{
    helper.createChain();
    const IptEntryPtr entry = RuleBuilder().setTarget(std::make_shared<UnknownTarget>()).build();
    helper.appendEntry(entry.get());
}

class MangleKernelTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        if (!isolationFailure().empty())
        {
            GTEST_SKIP() << "no isolated network namespace, the host tables are left alone: "
                         << isolationFailure();
        }
        try
        {
            IptcHelper probe(MANGLE_TABLE, "bb_probe");
            static_cast<void>(probe.chainExists());
        }
        catch (const IptcException& unavailable)
        {
            GTEST_SKIP() << "the mangle table is not available: " << unavailable.what();
        }
    }
};

} // namespace

TEST_F(MangleKernelTest, CreateChainIsSafeToRepeatAcrossActivations)
{
    MangleAdapter first("bb_k_twice");
    first.createChain();
    first.commit();

    MangleAdapter second("bb_k_twice");
    EXPECT_NO_THROW(second.createChain());
    EXPECT_NO_THROW(second.commit());

    IptcHelper helper(MANGLE_TABLE, "bb_k_twice");
    EXPECT_TRUE(helper.chainExists());
}

TEST_F(MangleKernelTest, MissingChainReadsAsNoRules)
{
    EXPECT_TRUE(readFromKernel("bb_k_missing").empty());
}

TEST_F(MangleKernelTest, ReadSkipsRulesThatAreNotNflog)
{
    MangleAdapter adapter("bb_k_mixed");
    adapter.createChain();
    adapter.addConntrackRule(MarkParam{SAMPLE_MARK}, ConntrackParam{ConntrackMatch::State::NEW});
    adapter.addNflogRule(MarkParam{SAMPLE_MARK}, fullNflog());
    adapter.commit();

    IptcHelper helper(MANGLE_TABLE, "bb_k_mixed");
    EXPECT_EQ(helper.listEntries().size(), 2U);
    EXPECT_EQ(readFromKernel("bb_k_mixed"), std::vector<NflogRuleParam>{marked(fullNflog())});
}

TEST_F(MangleKernelTest, NflogRuleWithoutMarkReadsBackWithoutMark)
{
    IptcHelper helper(MANGLE_TABLE, "bb_k_unmarked");
    helper.createChain();
    const auto entry = RuleBuilder().setTarget(std::make_shared<NflogTarget>(fullNflog())).build();
    helper.appendEntry(entry.get());
    helper.commit();

    EXPECT_EQ(readFromKernel("bb_k_unmarked"),
              (std::vector<NflogRuleParam>{{.mark = std::nullopt, .nflog = fullNflog()}}));
}

TEST_F(MangleKernelTest, UncommittedRulesAreNotInTheKernel)
{
    MangleAdapter writer("bb_k_pending");
    writer.createChain();
    writer.addNflogRule(MarkParam{SAMPLE_MARK}, fullNflog());

    EXPECT_TRUE(readFromKernel("bb_k_pending").empty());
}

TEST_F(MangleKernelTest, KernelRejectionThrowsWithErrnoAndChangesNothing)
{
    IptcHelper helper(MANGLE_TABLE, "bb_k_reject");
    appendUnknownTarget(helper);

    try
    {
        helper.commit();
        FAIL() << "the kernel accepted a target that does not exist";
    }
    catch (const IptcException& rejected)
    {
        EXPECT_NE(rejected.errorNumber(), 0) << rejected.what();
    }

    EXPECT_FALSE(helper.chainExists());
}

TEST_F(MangleKernelTest, HelperIsUsableAfterAFailedCommit)
{
    IptcHelper helper(MANGLE_TABLE, "bb_k_retry");
    appendUnknownTarget(helper);
    EXPECT_THROW(helper.commit(), IptcException);

    helper.createChain();
    EXPECT_NO_THROW(helper.commit());
    EXPECT_TRUE(helper.chainExists());
}

TEST_F(MangleKernelTest, ListingAMissingChainThrowsEnoent)
{
    IptcHelper helper(MANGLE_TABLE, "bb_k_nochain");

    try
    {
        static_cast<void>(helper.listEntries());
        FAIL() << "listed a chain that does not exist";
    }
    catch (const IptcException& missing)
    {
        EXPECT_EQ(missing.errorNumber(), ENOENT);
    }
}

TEST_F(MangleKernelTest, UnknownTableThrowsFromInitWithErrno)
{
    IptcHelper helper("bb_no_such_table", "bb_k_table");

    try
    {
        static_cast<void>(helper.chainExists());
        FAIL() << "opened a table that does not exist";
    }
    catch (const IptcException& unavailable)
    {
        EXPECT_NE(unavailable.errorNumber(), 0);
        EXPECT_NE(std::string(unavailable.what()).find("iptc_init(bb_no_such_table)"),
                  std::string::npos);
    }
}

/* LEARN: one scenario is the logging states applied to a chain, one activation each (a new
   adapter, a new libiptc snapshot), and what the kernel must hold afterwards. The chain always
   exists after an activation, and holds exactly the expected rules: nothing left over from an
   earlier state.
*/
struct ApplyCase
{
    const char* chain;
    std::vector<std::vector<NflogParam>> activations;
    std::vector<NflogRuleParam> expected;
    const char* name;
};

const std::vector<ApplyCase> APPLY_CASES{
    {"bb_k_add", {{fullNflog()}}, {marked(fullNflog())}, "AddedRuleReadsBackWithEveryField"},
    {"bb_k_reapply",
     {{fullNflog()}, {fullNflog()}},
     {marked(fullNflog())},
     "ReapplyingTheSameStateKeepsASingleRule"},
    {"bb_k_change",
     {{fullNflog()}, {changedNflog()}},
     {marked(changedNflog())},
     "ApplyingNewKnobsReplacesTheOldRule"},
    {"bb_k_remove", {{fullNflog()}, {}}, {}, "FlushRemovesTheRuleAndKeepsTheChain"},
    {"bb_k_order",
     {{fullNflog(), changedNflog()}},
     {marked(fullNflog()), marked(changedNflog())},
     "SeveralRulesKeepTheirOrder"},
    {"bb_k_utf8",
     {{multiByteNflog()}},
     {marked(
         withPrefix(multiByteNflog(), repeated(TWO_BYTE_CHARACTER, TWO_BYTE_CHARACTERS_THAT_FIT)))},
     "MultiBytePrefixIsCutOnACharacterBoundary"},
};

class MangleKernelApplyTest : public MangleKernelTest,
                              public ::testing::WithParamInterface<ApplyCase>
{
};

TEST_P(MangleKernelApplyTest, KernelHoldsExactlyTheLastAppliedState)
{
    const std::string chain = GetParam().chain;
    for (const std::vector<NflogParam>& state : GetParam().activations)
    {
        applyLogging(chain, state);
    }

    EXPECT_EQ(readFromKernel(chain), GetParam().expected);
    IptcHelper helper(MANGLE_TABLE, chain);
    EXPECT_TRUE(helper.chainExists());
    EXPECT_EQ(helper.listEntries().size(), GetParam().expected.size());
}

INSTANTIATE_TEST_SUITE_P(MangleKernel, MangleKernelApplyTest, ::testing::ValuesIn(APPLY_CASES),
                         caseName<ApplyCase>);
