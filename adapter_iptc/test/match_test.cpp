#include "adapter_iptc/builder/rule_builder.h"
#include "adapter_iptc/dtos/mark.h"
#include "adapter_iptc/ipt_entry_ptr.h"
#include "adapter_iptc/matchs/conntrack_match.h"
#include "adapter_iptc/matchs/hashlimit_match.h"
#include "adapter_iptc/matchs/mark_match.h"
#include "adapter_iptc/matchs/tcp_match.h"
#include "adapter_iptc/matchs/udp_match.h"
#include "adapter_iptc/targets/standard_target.h"
#include "parameterized_case.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <linux/in.h>
#include <linux/netfilter/nf_conntrack_common.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_conntrack.h>
#include <linux/netfilter/xt_hashlimit.h>
#include <linux/netfilter/xt_mark.h>
#include <linux/netfilter/xt_tcpudp.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr uint8_t ZERO_FILL_BYTE = 0;
constexpr uint32_t OTHER_MARK = 0x5678;

constexpr uint32_t SAMPLE_MARK = 0x1234;
constexpr uint32_t SAMPLE_MARK_MASK = 0xff00;
constexpr uint32_t FULL_MARK_MASK = 0xffffffff;
constexpr const char* MARK_NAME = "mark";
constexpr uint8_t MARK_REVISION = 1;

constexpr uint16_t SAMPLE_PORT = 8080;
constexpr uint16_t SAMPLE_PORT_RANGE_MIN = 1000;
constexpr uint16_t SAMPLE_PORT_RANGE_MAX = 2000;
constexpr uint16_t FULL_PORT_RANGE_MIN = 0;
constexpr uint16_t FULL_PORT_RANGE_MAX = 65535;
constexpr const char* TCP_NAME = "tcp";
constexpr const char* UDP_NAME = "udp";
constexpr uint8_t PORT_MATCH_REVISION = 0;
constexpr uint8_t NO_PROTOCOL = 0;

constexpr const char* CONNTRACK_NAME = "conntrack";
constexpr uint8_t CONNTRACK_REVISION = 3;

constexpr const char* HASHLIMIT_NAME = "hashlimit";
constexpr uint8_t HASHLIMIT_REVISION = 1;
constexpr const char* SAMPLE_HASHLIMIT_NAME = "limit-ssh";
constexpr uint32_t SAMPLE_HASHLIMIT_AVERAGE = 300;
constexpr uint32_t SAMPLE_HASHLIMIT_BURST = 5;
constexpr uint32_t SAMPLE_HASHLIMIT_EXPIRE = 60000;
constexpr uint32_t DEFAULT_GARBAGE_COLLECTION_INTERVAL = 1000;
constexpr uint8_t DEFAULT_HOST_MASK = 32;
constexpr uint32_t DEFAULT_HASHLIMIT_MODE =
    XT_HASHLIMIT_HASH_SIP | XT_HASHLIMIT_HASH_DIP | XT_HASHLIMIT_HASH_DPT;
constexpr uint32_t EXPLICIT_HASHLIMIT_MODE = XT_HASHLIMIT_HASH_DIP;

const xt_entry_match* headerOf(const std::vector<unsigned char>& buffer)
{
    return reinterpret_cast<const xt_entry_match*>(buffer.data());
}

std::shared_ptr<HashlimitMatch> sampleHashlimit()
{
    return std::make_shared<HashlimitMatch>(HashlimitParam{.name = SAMPLE_HASHLIMIT_NAME,
                                                           .average = SAMPLE_HASHLIMIT_AVERAGE,
                                                           .burst = SAMPLE_HASHLIMIT_BURST,
                                                           .expire = SAMPLE_HASHLIMIT_EXPIRE});
}

std::vector<unsigned char> writeToZeroedBuffer(const TemplateMatch& match)
{
    std::vector<unsigned char> buffer(match.size(), ZERO_FILL_BYTE);
    match.write(buffer.data());
    return buffer;
}

/* LEARN: xt_tcp and xt_udp are different structs that both start with the source and the
   destination port bounds; each case reads them through its own struct.
*/
struct PortBounds
{
    uint16_t sourceMin;
    uint16_t sourceMax;
    uint16_t destinationMin;
    uint16_t destinationMax;
};

PortBounds tcpPortsOf(const unsigned char* data)
{
    const auto* info = reinterpret_cast<const xt_tcp*>(data);
    return {info->spts[0], info->spts[1], info->dpts[0], info->dpts[1]};
}

PortBounds udpPortsOf(const unsigned char* data)
{
    const auto* info = reinterpret_cast<const xt_udp*>(data);
    return {info->spts[0], info->spts[1], info->dpts[0], info->dpts[1]};
}

IptEntryPtr acceptEntryWith(const std::vector<std::shared_ptr<TemplateMatch>>& matches)
{
    RuleBuilder builder;
    for (const std::shared_ptr<TemplateMatch>& match : matches)
    {
        builder.setMatch(match);
    }
    return builder.setTarget(std::make_shared<StandardTarget>(StandardTarget::Verdict::ACCEPT))
        .build();
}

IptEntryPtr markEntry()
{
    return acceptEntryWith({std::make_shared<MarkMatch>(SAMPLE_MARK)});
}

xt_entry_match* firstMatchOf(ipt_entry& entry)
{
    return reinterpret_cast<xt_entry_match*>(entry.elems);
}

xt_mark_mtinfo1* markInfoOf(ipt_entry& entry)
{
    return reinterpret_cast<xt_mark_mtinfo1*>(firstMatchOf(entry)->data);
}

} // namespace

/* LEARN: every match starts with the same xt_entry_match header (name, revision, size) followed
   by its own payload; tcp and udp also set the rule's protocol, the others leave it to 0.
*/
struct MatchHeaderCase
{
    std::shared_ptr<TemplateMatch> (*makeMatch)();
    const char* expectedName;
    uint8_t expectedRevision;
    std::size_t expectedPayloadBytes;
    uint8_t expectedProtocol;
    const char* name;
};

const std::vector<MatchHeaderCase> MATCH_HEADER_CASES{
    {[]() -> std::shared_ptr<TemplateMatch> { return std::make_shared<MarkMatch>(SAMPLE_MARK); },
     MARK_NAME, MARK_REVISION, sizeof(xt_mark_mtinfo1), NO_PROTOCOL, "Mark"},
    {[]() -> std::shared_ptr<TemplateMatch> { return std::make_shared<TcpMatch>(SAMPLE_PORT); },
     TCP_NAME, PORT_MATCH_REVISION, sizeof(xt_tcp), IPPROTO_TCP, "Tcp"},
    {[]() -> std::shared_ptr<TemplateMatch> { return std::make_shared<UdpMatch>(SAMPLE_PORT); },
     UDP_NAME, PORT_MATCH_REVISION, sizeof(xt_udp), IPPROTO_UDP, "Udp"},
    {[]() -> std::shared_ptr<TemplateMatch> {
         return std::make_shared<ConntrackMatch>(ConntrackMatch::State::ESTABLISHED_RELATED);
     },
     CONNTRACK_NAME, CONNTRACK_REVISION, sizeof(xt_conntrack_mtinfo3), NO_PROTOCOL, "Conntrack"},
    {[]() -> std::shared_ptr<TemplateMatch> { return sampleHashlimit(); }, HASHLIMIT_NAME,
     HASHLIMIT_REVISION, sizeof(xt_hashlimit_mtinfo1), NO_PROTOCOL, "Hashlimit"},
};

class MatchHeaderTest : public ::testing::TestWithParam<MatchHeaderCase>
{
};

TEST_P(MatchHeaderTest, SizeCoversHeaderAndPayload)
{
    const std::shared_ptr<TemplateMatch> match = GetParam().makeMatch();
    const std::size_t unalignedBytes = sizeof(xt_entry_match) + GetParam().expectedPayloadBytes;

    EXPECT_EQ(match->size(), XT_ALIGN(unalignedBytes));
}

TEST_P(MatchHeaderTest, WriteFillsHeaderWithNameRevisionAndSize)
{
    const std::shared_ptr<TemplateMatch> match = GetParam().makeMatch();
    const auto buffer = writeToZeroedBuffer(*match);
    const xt_entry_match* header = headerOf(buffer);

    EXPECT_STREQ(header->u.user.name, GetParam().expectedName);
    EXPECT_EQ(header->u.user.revision, GetParam().expectedRevision);
    EXPECT_EQ(static_cast<std::size_t>(header->u.user.match_size), match->size());
}

TEST_P(MatchHeaderTest, DeclaresItsProtocol)
{
    const std::shared_ptr<TemplateMatch> match = GetParam().makeMatch();

    EXPECT_EQ(match->protocol(), GetParam().expectedProtocol);
}

INSTANTIATE_TEST_SUITE_P(Match, MatchHeaderTest, ::testing::ValuesIn(MATCH_HEADER_CASES),
                         caseName<MatchHeaderCase>);

struct MarkWriteCase
{
    MarkMatch match;
    uint32_t expectedMark;
    uint32_t expectedMask;
    const char* name;
};

const std::vector<MarkWriteCase> MARK_WRITE_CASES{
    {MarkMatch(MaskedMark{SAMPLE_MARK, SAMPLE_MARK_MASK}), SAMPLE_MARK, SAMPLE_MARK_MASK,
     "ExplicitMask"},
    {MarkMatch(SAMPLE_MARK), SAMPLE_MARK, FULL_MARK_MASK, "DefaultMaskCoversAllMarkBits"},
};

class MarkWriteTest : public ::testing::TestWithParam<MarkWriteCase>
{
};

TEST_P(MarkWriteTest, StoresMarkAndMask)
{
    const auto buffer = writeToZeroedBuffer(GetParam().match);
    const auto* info = reinterpret_cast<const xt_mark_mtinfo1*>(headerOf(buffer)->data);

    EXPECT_EQ(info->mark, GetParam().expectedMark);
    EXPECT_EQ(info->mask, GetParam().expectedMask);
}

INSTANTIATE_TEST_SUITE_P(MarkMatch, MarkWriteTest, ::testing::ValuesIn(MARK_WRITE_CASES),
                         caseName<MarkWriteCase>);

/* LEARN: tcp and udp match only destination ports; the source bounds always span every port.
   A single port is written as a range of one.
*/
struct PortCase
{
    std::shared_ptr<TemplateMatch> (*makeMatch)();
    PortBounds (*portsOf)(const unsigned char* data);
    uint16_t expectedDestinationMin;
    uint16_t expectedDestinationMax;
    const char* name;
};

const std::vector<PortCase> PORT_CASES{
    {[]() -> std::shared_ptr<TemplateMatch> { return std::make_shared<TcpMatch>(SAMPLE_PORT); },
     tcpPortsOf, SAMPLE_PORT, SAMPLE_PORT, "TcpSinglePort"},
    {[]() -> std::shared_ptr<TemplateMatch> {
         return std::make_shared<TcpMatch>(PortRange{SAMPLE_PORT_RANGE_MIN, SAMPLE_PORT_RANGE_MAX});
     },
     tcpPortsOf, SAMPLE_PORT_RANGE_MIN, SAMPLE_PORT_RANGE_MAX, "TcpPortRange"},
    {[]() -> std::shared_ptr<TemplateMatch> { return std::make_shared<UdpMatch>(SAMPLE_PORT); },
     udpPortsOf, SAMPLE_PORT, SAMPLE_PORT, "UdpSinglePort"},
    {[]() -> std::shared_ptr<TemplateMatch> {
         return std::make_shared<UdpMatch>(PortRange{SAMPLE_PORT_RANGE_MIN, SAMPLE_PORT_RANGE_MAX});
     },
     udpPortsOf, SAMPLE_PORT_RANGE_MIN, SAMPLE_PORT_RANGE_MAX, "UdpPortRange"},
};

class PortMatchTest : public ::testing::TestWithParam<PortCase>
{
};

TEST_P(PortMatchTest, WritesDestinationBoundsAndEverySourcePort)
{
    const std::shared_ptr<TemplateMatch> match = GetParam().makeMatch();
    const auto buffer = writeToZeroedBuffer(*match);
    const PortBounds ports = GetParam().portsOf(headerOf(buffer)->data);

    EXPECT_EQ(ports.destinationMin, GetParam().expectedDestinationMin);
    EXPECT_EQ(ports.destinationMax, GetParam().expectedDestinationMax);
    EXPECT_EQ(ports.sourceMin, FULL_PORT_RANGE_MIN);
    EXPECT_EQ(ports.sourceMax, FULL_PORT_RANGE_MAX);
}

INSTANTIATE_TEST_SUITE_P(PortMatch, PortMatchTest, ::testing::ValuesIn(PORT_CASES),
                         caseName<PortCase>);

struct ConntrackStateCase
{
    ConntrackMatch::State state;
    uint16_t expectedFlags;
    uint16_t expectedStateMask;
    const char* name;
};

const std::vector<ConntrackStateCase> CONNTRACK_STATE_CASES{
    {ConntrackMatch::State::NEW, XT_CONNTRACK_STATE | XT_CONNTRACK_DIRECTION,
     NF_CT_STATE_BIT(IP_CT_NEW), "NewWithDirectionFlag"},
    {ConntrackMatch::State::ESTABLISHED_RELATED, XT_CONNTRACK_STATE,
     NF_CT_STATE_BIT(IP_CT_ESTABLISHED) | NF_CT_STATE_BIT(IP_CT_RELATED), "EstablishedRelated"},
    {ConntrackMatch::State::INVALID, XT_CONNTRACK_STATE, NF_CT_STATE_INVALID_BIT, "Invalid"},
};

class ConntrackStateTest : public ::testing::TestWithParam<ConntrackStateCase>
{
};

TEST_P(ConntrackStateTest, WritesFlagsAndStateBits)
{
    const ConntrackMatch match(GetParam().state);
    const auto buffer = writeToZeroedBuffer(match);
    const auto* info = reinterpret_cast<const xt_conntrack_mtinfo3*>(headerOf(buffer)->data);

    EXPECT_EQ(info->match_flags, GetParam().expectedFlags);
    EXPECT_EQ(info->state_mask, GetParam().expectedStateMask);
}

INSTANTIATE_TEST_SUITE_P(ConntrackMatch, ConntrackStateTest,
                         ::testing::ValuesIn(CONNTRACK_STATE_CASES), caseName<ConntrackStateCase>);

TEST(HashlimitMatchTest, WriteFillsNamedRateConfigWithDefaults)
{
    const auto buffer = writeToZeroedBuffer(*sampleHashlimit());
    const auto* info = reinterpret_cast<const xt_hashlimit_mtinfo1*>(headerOf(buffer)->data);

    EXPECT_STREQ(info->name, SAMPLE_HASHLIMIT_NAME);
    EXPECT_EQ(info->cfg.avg, SAMPLE_HASHLIMIT_AVERAGE);
    EXPECT_EQ(info->cfg.burst, SAMPLE_HASHLIMIT_BURST);
    EXPECT_EQ(info->cfg.expire, SAMPLE_HASHLIMIT_EXPIRE);
    EXPECT_EQ(info->cfg.gc_interval, DEFAULT_GARBAGE_COLLECTION_INTERVAL);
    EXPECT_EQ(info->cfg.srcmask, DEFAULT_HOST_MASK);
    EXPECT_EQ(info->cfg.dstmask, DEFAULT_HOST_MASK);
    EXPECT_EQ(info->cfg.mode, DEFAULT_HASHLIMIT_MODE);
}

TEST(HashlimitMatchTest, ExplicitModeIsWrittenVerbatim)
{
    const HashlimitMatch match(
        HashlimitParam{.name = SAMPLE_HASHLIMIT_NAME,
                       .average = SAMPLE_HASHLIMIT_AVERAGE,
                       .burst = SAMPLE_HASHLIMIT_BURST,
                       .expire = SAMPLE_HASHLIMIT_EXPIRE,
                       .garbageCollectionInterval = DEFAULT_GARBAGE_COLLECTION_INTERVAL,
                       .sourceMask = DEFAULT_HOST_MASK,
                       .destinationMask = DEFAULT_HOST_MASK,
                       .mode = EXPLICIT_HASHLIMIT_MODE});
    const auto buffer = writeToZeroedBuffer(match);
    const auto* info = reinterpret_cast<const xt_hashlimit_mtinfo1*>(headerOf(buffer)->data);

    EXPECT_EQ(info->cfg.mode, EXPLICIT_HASHLIMIT_MODE);
}

/* LEARN: one rule buffer and what MarkMatch::read() must make of it. makeEntry builds the rule,
   and may then edit its bytes to play a kernel value RuleBuilder never writes. expected is
   nullopt when the rule has no plain mark match, or its match headers cannot be trusted.
*/
struct MarkReadCase
{
    IptEntryPtr (*makeEntry)();
    std::optional<MarkParam> expected;
    const char* name;
};

const std::vector<MarkReadCase> MARK_DECODED_CASES{
    {markEntry, MarkParam{SAMPLE_MARK}, "MarkWrittenByMarkMatchRoundTrips"},
    {[]() {
         return acceptEntryWith({std::make_shared<ConntrackMatch>(ConntrackMatch::State::NEW),
                                 std::make_shared<MarkMatch>(SAMPLE_MARK)});
     },
     MarkParam{SAMPLE_MARK}, "MarkAfterAnotherMatchIsFound"},
    {[]() {
         return acceptEntryWith(
             {std::make_shared<MarkMatch>(SAMPLE_MARK), std::make_shared<MarkMatch>(OTHER_MARK)});
     },
     MarkParam{SAMPLE_MARK}, "FirstOfTwoMarkMatchesIsRead"},
};

const std::vector<MarkReadCase> MARK_REJECTED_CASES{
    {[]() {
         return acceptEntryWith({std::make_shared<ConntrackMatch>(ConntrackMatch::State::NEW)});
     },
     std::nullopt, "EntryWithoutMarkMatchHasNoMark"},
    {[]() { return acceptEntryWith({}); }, std::nullopt, "EntryWithoutMatchesHasNoMark"},
    {[]() {
         return acceptEntryWith(
             {std::make_shared<MarkMatch>(MaskedMark{SAMPLE_MARK, SAMPLE_MARK_MASK})});
     },
     std::nullopt, "PartialMaskIsNotAPlainMark"},
    {[]() {
         IptEntryPtr entry = markEntry();
         markInfoOf(*entry)->invert = 1;
         return entry;
     },
     std::nullopt, "InvertedMarkIsNotAPlainMark"},
    {[]() {
         IptEntryPtr entry = markEntry();
         firstMatchOf(*entry)->u.user.revision = 0;
         return entry;
     },
     std::nullopt, "OtherRevisionHasAnotherLayoutAndIsNotRead"},
    {[]() {
         IptEntryPtr entry = markEntry();
         firstMatchOf(*entry)->u.user.match_size = sizeof(xt_entry_match);
         return entry;
     },
     std::nullopt, "MatchSmallerThanMarkInfoIsRejected"},
    {[]() {
         IptEntryPtr entry = markEntry();
         firstMatchOf(*entry)->u.user.match_size = 0;
         return entry;
     },
     std::nullopt, "ZeroMatchSizeStopsTheWalk"},
    {[]() {
         IptEntryPtr entry = markEntry();
         firstMatchOf(*entry)->u.user.match_size = entry->target_offset;
         return entry;
     },
     std::nullopt, "MatchRunningIntoTheTargetIsRejected"},
    {[]() {
         IptEntryPtr entry = markEntry();
         const std::size_t pastTheEntry = entry->next_offset + 1;
         entry->target_offset = pastTheEntry;
         return entry;
     },
     std::nullopt, "TargetOffsetPastTheEntryIsRejected"},
    {[]() {
         IptEntryPtr entry = markEntry();
         std::memset(firstMatchOf(*entry)->u.user.name, 'm',
                     sizeof(firstMatchOf(*entry)->u.user.name));
         return entry;
     },
     std::nullopt, "MatchNameWithoutTerminatorIsRejected"},
};

class MarkReadTest : public ::testing::TestWithParam<MarkReadCase>
{
};

TEST_P(MarkReadTest, DecodesWhatTheRuleHolds)
{
    const IptEntryPtr entry = GetParam().makeEntry();

    EXPECT_EQ(MarkMatch::read(*entry), GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(Decoded, MarkReadTest, ::testing::ValuesIn(MARK_DECODED_CASES),
                         caseName<MarkReadCase>);

INSTANTIATE_TEST_SUITE_P(Rejected, MarkReadTest, ::testing::ValuesIn(MARK_REJECTED_CASES),
                         caseName<MarkReadCase>);
