#include "adapter_iptc/builder/rule_builder.h"

#include "adapter_iptc/ipt_entry_ptr.h"
#include "adapter_iptc/matchs/mark_match.h"
#include "adapter_iptc/matchs/tcp_match.h"
#include "adapter_iptc/matchs/udp_match.h"
#include "adapter_iptc/targets/nflog_target.h"
#include "adapter_iptc/targets/standard_target.h"
#include "parameterized_case.h"

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <linux/in.h>
#include <linux/netfilter.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_NFLOG.h>
#include <linux/netfilter/xt_mark.h>
#include <linux/netfilter/xt_tcpudp.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <memory>
#include <vector>

namespace {

constexpr uint32_t SAMPLE_MARK = 0x1234;
constexpr uint16_t SAMPLE_PORT = 8080;
constexpr uint16_t SAMPLE_NFLOG_GROUP = 3;
constexpr const char* SAMPLE_NFLOG_PREFIX = "bb-test";

constexpr const char* MARK_NAME = "mark";
constexpr const char* TCP_NAME = "tcp";
constexpr const char* NFLOG_NAME = "NFLOG";
constexpr uint8_t MARK_REVISION = 1;
constexpr uint8_t PORT_MATCH_REVISION = 0;
constexpr uint8_t NO_PROTOCOL = 0;
constexpr int ACCEPT_VERDICT = -NF_ACCEPT - 1;

constexpr std::size_t MARK_MATCH_BYTES = XT_ALIGN(sizeof(xt_entry_match) + sizeof(xt_mark_mtinfo1));
constexpr std::size_t TCP_MATCH_BYTES = XT_ALIGN(sizeof(xt_entry_match) + sizeof(xt_tcp));
constexpr std::size_t UDP_MATCH_BYTES = XT_ALIGN(sizeof(xt_entry_match) + sizeof(xt_udp));
constexpr std::size_t STANDARD_TARGET_BYTES = XT_ALIGN(sizeof(xt_standard_target));
constexpr std::size_t NFLOG_TARGET_BYTES =
    XT_ALIGN(sizeof(xt_entry_target) + sizeof(xt_nflog_info));

const unsigned char* bytesOf(const ipt_entry& entry)
{
    return reinterpret_cast<const unsigned char*>(&entry);
}

const xt_entry_match* matchAt(const ipt_entry& entry, std::size_t offset)
{
    return reinterpret_cast<const xt_entry_match*>(
        reinterpret_cast<const unsigned char*>(entry.elems) + offset);
}

const xt_entry_target* targetAt(const ipt_entry& entry)
{
    return reinterpret_cast<const xt_entry_target*>(bytesOf(entry) + entry.target_offset);
}

std::shared_ptr<StandardTarget> accept()
{
    return std::make_shared<StandardTarget>(StandardTarget::Verdict::ACCEPT);
}

} // namespace

/* LEARN: one rule and the layout RuleBuilder must give it: the matches fill the space between the
   header and target_offset, the target fills the space up to next_offset, and ip.proto comes
   from the first match that declares a protocol (tcp/udp), or stays 0.
*/
struct RuleLayoutCase
{
    IptEntryPtr (*build)();
    std::size_t expectedMatchesBytes;
    std::size_t expectedTargetBytes;
    uint8_t expectedProtocol;
    const char* name;
};

const std::vector<RuleLayoutCase> RULE_LAYOUT_CASES{
    {[]() { return RuleBuilder().setTarget(accept()).build(); }, 0, STANDARD_TARGET_BYTES,
     NO_PROTOCOL, "NoMatchStandardTarget"},
    {[]() {
         return RuleBuilder()
             .setMatch(std::make_shared<MarkMatch>(SAMPLE_MARK))
             .setTarget(accept())
             .build();
     },
     MARK_MATCH_BYTES, STANDARD_TARGET_BYTES, NO_PROTOCOL, "MarkStandardTarget"},
    {[]() {
         return RuleBuilder()
             .setMatch(std::make_shared<MarkMatch>(SAMPLE_MARK))
             .setMatch(std::make_shared<TcpMatch>(SAMPLE_PORT))
             .setTarget(accept())
             .build();
     },
     MARK_MATCH_BYTES + TCP_MATCH_BYTES, STANDARD_TARGET_BYTES, IPPROTO_TCP,
     "MarkTcpStandardTarget"},
    {[]() {
         return RuleBuilder()
             .setMatch(std::make_shared<UdpMatch>(SAMPLE_PORT))
             .setTarget(accept())
             .build();
     },
     UDP_MATCH_BYTES, STANDARD_TARGET_BYTES, IPPROTO_UDP, "UdpStandardTarget"},
    {[]() {
         return RuleBuilder()
             .setMatch(std::make_shared<MarkMatch>(SAMPLE_MARK))
             .setMatch(std::make_shared<TcpMatch>(SAMPLE_PORT))
             .setTarget(std::make_shared<NflogTarget>(SAMPLE_NFLOG_GROUP, SAMPLE_NFLOG_PREFIX))
             .build();
     },
     MARK_MATCH_BYTES + TCP_MATCH_BYTES, NFLOG_TARGET_BYTES, IPPROTO_TCP, "MarkTcpNflogTarget"},
};

class RuleLayoutTest : public ::testing::TestWithParam<RuleLayoutCase>
{
};

TEST_P(RuleLayoutTest, TargetOffsetCoversHeaderAndEveryMatch)
{
    const IptEntryPtr entry = GetParam().build();
    const std::size_t expectedTargetOffset = sizeof(ipt_entry) + GetParam().expectedMatchesBytes;

    EXPECT_EQ(static_cast<std::size_t>(entry->target_offset), expectedTargetOffset);
}

TEST_P(RuleLayoutTest, NextOffsetIsTargetOffsetPlusTargetSize)
{
    const IptEntryPtr entry = GetParam().build();
    const std::size_t targetOffset = entry->target_offset;
    const std::size_t expectedNextOffset = targetOffset + GetParam().expectedTargetBytes;

    EXPECT_EQ(static_cast<std::size_t>(entry->next_offset), expectedNextOffset);
}

TEST_P(RuleLayoutTest, ProtocolComesFromTheFirstProtocolMatch)
{
    const IptEntryPtr entry = GetParam().build();

    EXPECT_EQ(entry->ip.proto, GetParam().expectedProtocol);
}

INSTANTIATE_TEST_SUITE_P(RuleBuilder, RuleLayoutTest, ::testing::ValuesIn(RULE_LAYOUT_CASES),
                         caseName<RuleLayoutCase>);

TEST(RuleBuilderTest, MatchHeadersLandInInsertionOrder)
{
    const auto entry = RuleBuilder()
                           .setMatch(std::make_shared<MarkMatch>(SAMPLE_MARK))
                           .setMatch(std::make_shared<TcpMatch>(SAMPLE_PORT))
                           .setTarget(accept())
                           .build();

    const xt_entry_match* markHeader = matchAt(*entry, 0);
    const xt_entry_match* tcpHeader = matchAt(*entry, MARK_MATCH_BYTES);

    EXPECT_STREQ(markHeader->u.user.name, MARK_NAME);
    EXPECT_EQ(markHeader->u.user.revision, MARK_REVISION);
    EXPECT_EQ(static_cast<std::size_t>(markHeader->u.user.match_size), MARK_MATCH_BYTES);

    EXPECT_STREQ(tcpHeader->u.user.name, TCP_NAME);
    EXPECT_EQ(tcpHeader->u.user.revision, PORT_MATCH_REVISION);
    EXPECT_EQ(static_cast<std::size_t>(tcpHeader->u.user.match_size), TCP_MATCH_BYTES);
}

TEST(RuleBuilderTest, ExtensionTargetHeaderLandsAtTargetOffset)
{
    const auto entry =
        RuleBuilder()
            .setMatch(std::make_shared<MarkMatch>(SAMPLE_MARK))
            .setTarget(std::make_shared<NflogTarget>(SAMPLE_NFLOG_GROUP, SAMPLE_NFLOG_PREFIX))
            .build();

    const xt_entry_target* target = targetAt(*entry);

    EXPECT_STREQ(target->u.user.name, NFLOG_NAME);
    EXPECT_EQ(static_cast<std::size_t>(target->u.user.target_size), NFLOG_TARGET_BYTES);
}

TEST(RuleBuilderTest, StandardTargetVerdictLandsAtTargetOffset)
{
    const auto entry = RuleBuilder()
                           .setMatch(std::make_shared<MarkMatch>(SAMPLE_MARK))
                           .setTarget(accept())
                           .build();

    const auto* standard = reinterpret_cast<const xt_standard_target*>(targetAt(*entry));

    EXPECT_EQ(standard->verdict, ACCEPT_VERDICT);
}
