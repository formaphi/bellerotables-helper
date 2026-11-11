#include "adapter_iptc/targets/nflog_target.h"

#include "adapter_iptc/builder/rule_builder.h"
#include "adapter_iptc/dtos/nflog.h"
#include "adapter_iptc/exceptions/iptc_exception.h"
#include "adapter_iptc/ipt_entry_ptr.h"
#include "adapter_iptc/matchs/mark_match.h"
#include "adapter_iptc/targets/standard_target.h"
#include "parameterized_case.h"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_NFLOG.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr uint8_t ZERO_FILL_BYTE = 0;
constexpr uint32_t SAMPLE_MARK = 0x1234;
constexpr uint16_t SAMPLE_NFLOG_GROUP = 7;
constexpr const char* SAMPLE_NFLOG_PREFIX = "DNAT ";
constexpr uint32_t SAMPLE_NFLOG_RANGE = 128;
constexpr uint16_t SAMPLE_NFLOG_THRESHOLD = 10;
constexpr std::size_t KERNEL_PREFIX_BYTES = 63;
constexpr const char* TWO_BYTE_CHARACTER = "\xC3\xA9";
constexpr const char* THREE_BYTE_CHARACTER = "\xE2\x82\xAC";
constexpr const char* FOUR_BYTE_CHARACTER = "\xF0\x9F\x98\x80";
constexpr std::size_t KERNEL_PREFIX_FIELD_BYTES = 64;

std::string repeated(const std::string& unit, std::size_t times)
{
    std::string result;
    for (std::size_t index = 0; index < times; ++index)
    {
        result += unit;
    }
    return result;
}

NflogParam fullNflog()
{
    return NflogParam{.prefix = SAMPLE_NFLOG_PREFIX,
                      .group = SAMPLE_NFLOG_GROUP,
                      .range = SAMPLE_NFLOG_RANGE,
                      .threshold = SAMPLE_NFLOG_THRESHOLD};
}

NflogParam withoutRange(NflogParam nflog)
{
    nflog.range.reset();
    return nflog;
}

NflogParam withThreshold(NflogParam nflog, std::optional<uint16_t> threshold)
{
    nflog.threshold = threshold;
    return nflog;
}

NflogParam withPrefix(NflogParam nflog, std::string prefix)
{
    nflog.prefix = std::move(prefix);
    return nflog;
}

std::vector<unsigned char> writeToZeroedBuffer(const ITargetStrategy& target)
{
    std::vector<unsigned char> buffer(target.size(), ZERO_FILL_BYTE);
    target.write(buffer.data());
    return buffer;
}

const xt_nflog_info* infoOf(const std::vector<unsigned char>& buffer)
{
    return reinterpret_cast<const xt_nflog_info*>(
        reinterpret_cast<const xt_entry_target*>(buffer.data())->data);
}

IptEntryPtr entryWith(const NflogParam& nflog)
{
    return RuleBuilder()
        .setMatch(std::make_shared<MarkMatch>(SAMPLE_MARK))
        .setTarget(std::make_shared<NflogTarget>(nflog))
        .build();
}

xt_entry_target* targetOf(ipt_entry& entry)
{
    return reinterpret_cast<xt_entry_target*>(reinterpret_cast<unsigned char*>(&entry) +
                                              entry.target_offset);
}

xt_nflog_info* infoOf(ipt_entry& entry)
{
    return reinterpret_cast<xt_nflog_info*>(targetOf(entry)->data);
}

bool isValidUtf8(const std::string& text)
{
    std::size_t index = 0;
    while (index < text.size())
    {
        const auto lead = static_cast<unsigned char>(text[index]);
        std::size_t length = 0;
        if (lead < 0x80U)
            length = 1;
        else if ((lead & 0xE0U) == 0xC0U)
            length = 2;
        else if ((lead & 0xF0U) == 0xE0U)
            length = 3;
        else if ((lead & 0xF8U) == 0xF0U)
            length = 4;
        else
            return false;
        if (index + length > text.size())
            return false;
        for (std::size_t next = 1; next < length; ++next)
        {
            if ((static_cast<unsigned char>(text[index + next]) & 0xC0U) != 0x80U)
                return false;
        }
        index += length;
    }
    return true;
}

} // namespace

struct PrefixCase
{
    std::string prefix;
    std::string expectedInKernel;
    const char* name;
};

const std::vector<PrefixCase> PREFIX_CASES{
    {"", "", "Empty"},
    {SAMPLE_NFLOG_PREFIX, SAMPLE_NFLOG_PREFIX, "ShortWithTrailingSpace"},
    {repeated("a", KERNEL_PREFIX_BYTES), repeated("a", KERNEL_PREFIX_BYTES), "Exactly63Bytes"},
    {repeated("a", 64), repeated("a", KERNEL_PREFIX_BYTES), "AsciiCutAt63Bytes"},
    {repeated(TWO_BYTE_CHARACTER, 64), repeated(TWO_BYTE_CHARACTER, 31),
     "TwoByteCharactersCutBeforeTheSplitOne"},
    {repeated(THREE_BYTE_CHARACTER, 30), repeated(THREE_BYTE_CHARACTER, 21),
     "ThreeByteCharactersEndingExactlyAt63"},
    {repeated(FOUR_BYTE_CHARACTER, 20), repeated(FOUR_BYTE_CHARACTER, 15),
     "FourByteCharactersCutBeforeTheSplitOne"},
    {repeated("a", 62) + TWO_BYTE_CHARACTER, repeated("a", 62), "LastCharacterWouldStraddle63"},
};

class NflogPrefixTest : public ::testing::TestWithParam<PrefixCase>
{
};

TEST_P(NflogPrefixTest, KeepsWholeCharactersWithinTheKernelBudget)
{
    const NflogTarget target(NflogParam{.prefix = GetParam().prefix, .group = SAMPLE_NFLOG_GROUP});
    const auto buffer = writeToZeroedBuffer(target);

    const std::string written(infoOf(buffer)->prefix);

    EXPECT_EQ(written, GetParam().expectedInKernel);
    EXPECT_LE(written.size(), KERNEL_PREFIX_BYTES);
    EXPECT_TRUE(isValidUtf8(written));
}

INSTANTIATE_TEST_SUITE_P(NflogTarget, NflogPrefixTest, ::testing::ValuesIn(PREFIX_CASES),
                         caseName<PrefixCase>);

/* LEARN: how each NflogParam lands in xt_nflog_info. An unset range leaves len 0 without the
   XT_NFLOG_F_COPY_LEN flag, and an unset threshold leaves the kernel default: both mean "not set"
   to the kernel.
*/
struct NflogWriteCase
{
    NflogParam nflog;
    uint32_t expectedLength;
    uint16_t expectedFlags;
    uint16_t expectedThreshold;
    const char* name;
};

const std::vector<NflogWriteCase> NFLOG_WRITE_CASES{
    {NflogParam{.prefix = SAMPLE_NFLOG_PREFIX, .group = SAMPLE_NFLOG_GROUP}, 0, 0,
     XT_NFLOG_DEFAULT_THRESHOLD, "GroupAndPrefixOnly"},
    {fullNflog(), SAMPLE_NFLOG_RANGE, XT_NFLOG_F_COPY_LEN, SAMPLE_NFLOG_THRESHOLD, "EveryKnob"},
    {withThreshold(fullNflog(), std::nullopt), SAMPLE_NFLOG_RANGE, XT_NFLOG_F_COPY_LEN,
     XT_NFLOG_DEFAULT_THRESHOLD, "RangeOnly"},
    {withoutRange(fullNflog()), 0, 0, SAMPLE_NFLOG_THRESHOLD, "ThresholdOnly"},
};

class NflogWriteTest : public ::testing::TestWithParam<NflogWriteCase>
{
};

TEST_P(NflogWriteTest, WritesEveryKnobIntoTheKernelInfo)
{
    const NflogParam& nflog = GetParam().nflog;
    const auto buffer = writeToZeroedBuffer(NflogTarget(nflog));
    const xt_nflog_info* info = infoOf(buffer);

    EXPECT_EQ(info->group, nflog.group);
    EXPECT_STREQ(info->prefix, nflog.prefix.c_str());
    EXPECT_EQ(info->len, GetParam().expectedLength);
    EXPECT_EQ(info->flags, GetParam().expectedFlags);
    EXPECT_EQ(info->threshold, GetParam().expectedThreshold);
}

INSTANTIATE_TEST_SUITE_P(NflogTarget, NflogWriteTest, ::testing::ValuesIn(NFLOG_WRITE_CASES),
                         caseName<NflogWriteCase>);

TEST(NflogTargetTest, GroupAndPrefixConstructorMatchesTheParamConstructor)
{
    const auto fromFields =
        writeToZeroedBuffer(NflogTarget(SAMPLE_NFLOG_GROUP, SAMPLE_NFLOG_PREFIX));
    const auto fromParam = writeToZeroedBuffer(
        NflogTarget(NflogParam{.prefix = SAMPLE_NFLOG_PREFIX, .group = SAMPLE_NFLOG_GROUP}));

    EXPECT_EQ(fromFields, fromParam);
}

/* LEARN: one rule buffer and what NflogTarget::read() must make of it. makeEntry builds the rule,
   and may then edit its bytes to play a kernel value RuleBuilder never writes. expected is
   nullopt when the rule must not be decoded as NFLOG at all.
*/
struct NflogReadCase
{
    IptEntryPtr (*makeEntry)();
    std::optional<NflogParam> expected;
    const char* name;
};

const std::vector<NflogReadCase> NFLOG_DECODED_CASES{
    {[]() { return entryWith(fullNflog()); }, fullNflog(), "EveryFieldRoundTrips"},
    {[]() { return entryWith(NflogParam{.prefix = "", .group = 0}); },
     NflogParam{.prefix = "", .group = 0}, "UnsetRangeAndThresholdReadBackUnset"},
    {[]() { return entryWith(withThreshold(fullNflog(), 0)); },
     withThreshold(fullNflog(), std::nullopt), "ZeroThresholdIsTheKernelDefaultAndReadsBackUnset"},
    {[]() {
         IptEntryPtr entry = entryWith(fullNflog());
         infoOf(*entry)->flags = 0;
         return entry;
     },
     withoutRange(fullNflog()), "LengthWithoutCopyLenFlagIsNotInEffectAndReadsBackUnset"},
    {[]() {
         IptEntryPtr entry = entryWith(fullNflog());
         std::memset(infoOf(*entry)->prefix, 'x', KERNEL_PREFIX_FIELD_BYTES);
         return entry;
     },
     withPrefix(fullNflog(), repeated("x", KERNEL_PREFIX_FIELD_BYTES)),
     "PrefixFillingTheWholeFieldIsReadWithoutOverrun"},
};

const std::vector<NflogReadCase> NFLOG_REJECTED_CASES{
    {[]() {
         return RuleBuilder()
             .setTarget(std::make_shared<StandardTarget>(StandardTarget::Verdict::ACCEPT))
             .build();
     },
     std::nullopt, "StandardTargetIsNotNflog"},
    {[]() {
         IptEntryPtr entry = entryWith(fullNflog());
         char* name = targetOf(*entry)->u.user.name;
         std::strncpy(name, "NFQUEUE", sizeof(targetOf(*entry)->u.user.name));
         return entry;
     },
     std::nullopt, "OtherExtensionTargetIsNotNflog"},
    {[]() {
         IptEntryPtr entry = entryWith(fullNflog());
         std::memset(targetOf(*entry)->u.user.name, 'N', sizeof(targetOf(*entry)->u.user.name));
         return entry;
     },
     std::nullopt, "TargetNameWithoutTerminatorIsRejected"},
    {[]() {
         IptEntryPtr entry = entryWith(fullNflog());
         targetOf(*entry)->u.user.target_size = sizeof(xt_entry_target);
         return entry;
     },
     std::nullopt, "TargetTooSmallForNflogInfoIsRejected"},
    {[]() {
         IptEntryPtr entry = entryWith(fullNflog());
         const std::size_t targetHeaderEnd = entry->target_offset + sizeof(xt_entry_target);
         entry->next_offset = targetHeaderEnd;
         return entry;
     },
     std::nullopt, "TargetRunningPastTheEntryIsRejected"},
    {[]() {
         /* LEARN: a well-formed NFLOG target is copied to offset 0, over the rule header, and
            target_offset points at it. Every other check passes (the name is NFLOG, the size fits
            in the rule), so only the "target starts after the rule header" check can reject it.
         */
         static_assert(
             XT_ALIGN(sizeof(xt_entry_target) + sizeof(xt_nflog_info)) <= sizeof(ipt_entry),
             "the NFLOG target must fit over the rule header");
         IptEntryPtr entry = entryWith(fullNflog());
         const std::size_t entryEnd = entry->next_offset;
         const auto* realTarget = reinterpret_cast<const unsigned char*>(targetOf(*entry));
         const std::size_t targetBytes = targetOf(*entry)->u.user.target_size;
         const std::vector<unsigned char> targetCopy(realTarget, realTarget + targetBytes);

         std::memcpy(entry.get(), targetCopy.data(), targetBytes);
         entry->target_offset = 0;
         entry->next_offset = entryEnd;
         return entry;
     },
     std::nullopt, "TargetOffsetInsideTheHeaderIsRejected"},
};

class NflogReadTest : public ::testing::TestWithParam<NflogReadCase>
{
};

TEST_P(NflogReadTest, DecodesWhatTheRuleHolds)
{
    const IptEntryPtr entry = GetParam().makeEntry();

    EXPECT_EQ(NflogTarget::read(*entry), GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(Decoded, NflogReadTest, ::testing::ValuesIn(NFLOG_DECODED_CASES),
                         caseName<NflogReadCase>);

INSTANTIATE_TEST_SUITE_P(Rejected, NflogReadTest, ::testing::ValuesIn(NFLOG_REJECTED_CASES),
                         caseName<NflogReadCase>);

TEST(IptcExceptionTest, ErrorNumberIsZeroWhenNotGiven)
{
    const IptcException exception("message");

    EXPECT_EQ(exception.errorNumber(), 0);
    EXPECT_STREQ(exception.what(), "message");
}

TEST(IptcExceptionTest, KeepsTheErrorNumber)
{
    const IptcException exception("iptc_commit(mangle): Bad rule", EINVAL);

    EXPECT_EQ(exception.errorNumber(), EINVAL);
}
