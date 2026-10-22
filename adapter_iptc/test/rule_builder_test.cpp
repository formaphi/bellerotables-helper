#include <adapter_iptc/builder/rule_builder.h>

#include <adapter_iptc/matchs/mark_match.h>
#include <adapter_iptc/matchs/tcp_match.h>
#include <adapter_iptc/matchs/udp_match.h>
#include <adapter_iptc/targets/nflog_target.h>
#include <adapter_iptc/targets/standard_target.h>

#include <gtest/gtest.h>

#include <linux/netfilter.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_NFLOG.h>
#include <linux/netfilter/xt_mark.h>
#include <linux/netfilter/xt_tcpudp.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/in.h>

#include <cstddef>
#include <cstdint>
#include <memory>

namespace {

constexpr uint32_t SAMPLE_MARK = 0x1234;
constexpr uint16_t SAMPLE_PORT = 8080;
constexpr uint16_t SAMPLE_PORT_RANGE_MIN = 1000;
constexpr uint16_t SAMPLE_PORT_RANGE_MAX = 2000;
constexpr uint16_t SAMPLE_NFLOG_GROUP = 3;
constexpr const char* SAMPLE_NFLOG_PREFIX = "bb-test";

constexpr const char* MARK_NAME = "mark";
constexpr const char* TCP_NAME = "tcp";
constexpr const char* NFLOG_NAME = "NFLOG";
constexpr uint8_t MARK_REVISION = 1;
constexpr uint8_t PORT_MATCH_REVISION = 0;
constexpr uint8_t NO_PROTOCOL = 0;
constexpr int ACCEPT_VERDICT = -NF_ACCEPT - 1;

const unsigned char* bytesOf(const ipt_entry& entry)
{
    return reinterpret_cast<const unsigned char*>(&entry);
}

const xt_entry_match* matchAt(const ipt_entry& entry, std::size_t offset)
{
    return reinterpret_cast<const xt_entry_match*>(reinterpret_cast<const unsigned char*>(entry.elems) + offset);
}

const xt_entry_target* targetAt(const ipt_entry& entry)
{
    return reinterpret_cast<const xt_entry_target*>(bytesOf(entry) + entry.target_offset);
}

std::size_t alignedMatchBytes(std::size_t dataBytes)
{
    return XT_ALIGN(sizeof(xt_entry_match) + dataBytes);
}

} // namespace

TEST(RuleBuilderTest, TargetOffsetWithoutMatchesIsHeaderSize)
{
    const auto entry = RuleBuilder()
        .setTarget(std::make_shared<StandardTarget>(StandardTarget::Verdict::ACCEPT))
        .build();

    EXPECT_EQ(static_cast<std::size_t>(entry->target_offset), sizeof(ipt_entry));
}

TEST(RuleBuilderTest, TargetOffsetCoversHeaderAndOneMatch)
{
    const auto entry = RuleBuilder()
        .setMatch(std::make_shared<MarkMatch>(SAMPLE_MARK))
        .setTarget(std::make_shared<StandardTarget>(StandardTarget::Verdict::ACCEPT))
        .build();

    const std::size_t expectedOffset = sizeof(ipt_entry) + alignedMatchBytes(sizeof(xt_mark_mtinfo1));
    EXPECT_EQ(static_cast<std::size_t>(entry->target_offset), expectedOffset);
}

TEST(RuleBuilderTest, TargetOffsetCoversHeaderAndAllMatches)
{
    const auto entry = RuleBuilder()
        .setMatch(std::make_shared<MarkMatch>(SAMPLE_MARK))
        .setMatch(std::make_shared<TcpMatch>(SAMPLE_PORT))
        .setTarget(std::make_shared<StandardTarget>(StandardTarget::Verdict::ACCEPT))
        .build();

    const std::size_t expectedOffset = sizeof(ipt_entry)
        + alignedMatchBytes(sizeof(xt_mark_mtinfo1))
        + alignedMatchBytes(sizeof(xt_tcp));
    EXPECT_EQ(static_cast<std::size_t>(entry->target_offset), expectedOffset);
}

TEST(RuleBuilderTest, NextOffsetCoversStandardTargetWithoutMatches)
{
    const auto entry = RuleBuilder()
        .setTarget(std::make_shared<StandardTarget>(StandardTarget::Verdict::ACCEPT))
        .build();

    const std::size_t expectedNextOffset = sizeof(ipt_entry) + XT_ALIGN(sizeof(xt_standard_target));
    EXPECT_EQ(static_cast<std::size_t>(entry->next_offset), expectedNextOffset);
}

TEST(RuleBuilderTest, NextOffsetIsTargetOffsetPlusExtensionTargetSize)
{
    const auto entry = RuleBuilder()
        .setMatch(std::make_shared<MarkMatch>(SAMPLE_MARK))
        .setMatch(std::make_shared<TcpMatch>(SAMPLE_PORT))
        .setTarget(std::make_shared<NflogTarget>(SAMPLE_NFLOG_GROUP, SAMPLE_NFLOG_PREFIX))
        .build();

    const std::size_t targetBytes = XT_ALIGN(sizeof(xt_entry_target) + sizeof(xt_nflog_info));
    EXPECT_EQ(static_cast<std::size_t>(entry->next_offset),
        static_cast<std::size_t>(entry->target_offset) + targetBytes);
}

TEST(RuleBuilderTest, ProtocolIsTcpWhenTcpMatchPresent)
{
    const auto entry = RuleBuilder()
        .setMatch(std::make_shared<MarkMatch>(SAMPLE_MARK))
        .setMatch(std::make_shared<TcpMatch>(SAMPLE_PORT))
        .setTarget(std::make_shared<StandardTarget>(StandardTarget::Verdict::ACCEPT))
        .build();

    EXPECT_EQ(entry->ip.proto, IPPROTO_TCP);
}

TEST(RuleBuilderTest, ProtocolIsUdpWhenUdpMatchPresent)
{
    const auto entry = RuleBuilder()
        .setMatch(std::make_shared<UdpMatch>(SAMPLE_PORT))
        .setTarget(std::make_shared<StandardTarget>(StandardTarget::Verdict::ACCEPT))
        .build();

    EXPECT_EQ(entry->ip.proto, IPPROTO_UDP);
}

TEST(RuleBuilderTest, ProtocolIsZeroWithoutProtocolMatch)
{
    const auto entry = RuleBuilder()
        .setMatch(std::make_shared<MarkMatch>(SAMPLE_MARK))
        .setTarget(std::make_shared<StandardTarget>(StandardTarget::Verdict::ACCEPT))
        .build();

    EXPECT_EQ(entry->ip.proto, NO_PROTOCOL);
}

TEST(RuleBuilderTest, MatchHeadersLandInInsertionOrder)
{
    const auto entry = RuleBuilder()
        .setMatch(std::make_shared<MarkMatch>(SAMPLE_MARK))
        .setMatch(std::make_shared<TcpMatch>(SAMPLE_PORT))
        .setTarget(std::make_shared<StandardTarget>(StandardTarget::Verdict::ACCEPT))
        .build();

    const std::size_t markBytes = alignedMatchBytes(sizeof(xt_mark_mtinfo1));
    const xt_entry_match* markHeader = matchAt(*entry, 0);
    const xt_entry_match* tcpHeader = matchAt(*entry, markBytes);

    EXPECT_STREQ(markHeader->u.user.name, MARK_NAME);
    EXPECT_EQ(markHeader->u.user.revision, MARK_REVISION);
    EXPECT_EQ(static_cast<std::size_t>(markHeader->u.user.match_size), markBytes);

    EXPECT_STREQ(tcpHeader->u.user.name, TCP_NAME);
    EXPECT_EQ(tcpHeader->u.user.revision, PORT_MATCH_REVISION);
    EXPECT_EQ(static_cast<std::size_t>(tcpHeader->u.user.match_size), alignedMatchBytes(sizeof(xt_tcp)));
}

TEST(RuleBuilderTest, ExtensionTargetHeaderLandsAtTargetOffset)
{
    const auto entry = RuleBuilder()
        .setMatch(std::make_shared<MarkMatch>(SAMPLE_MARK))
        .setTarget(std::make_shared<NflogTarget>(SAMPLE_NFLOG_GROUP, SAMPLE_NFLOG_PREFIX))
        .build();

    const xt_entry_target* target = targetAt(*entry);

    EXPECT_STREQ(target->u.user.name, NFLOG_NAME);
    EXPECT_EQ(static_cast<std::size_t>(target->u.user.target_size),
        XT_ALIGN(sizeof(xt_entry_target) + sizeof(xt_nflog_info)));
}

TEST(RuleBuilderTest, StandardTargetVerdictLandsAtTargetOffset)
{
    const auto entry = RuleBuilder()
        .setMatch(std::make_shared<MarkMatch>(SAMPLE_MARK))
        .setTarget(std::make_shared<StandardTarget>(StandardTarget::Verdict::ACCEPT))
        .build();

    const auto* standard = reinterpret_cast<const xt_standard_target*>(targetAt(*entry));

    EXPECT_EQ(standard->verdict, ACCEPT_VERDICT);
}
