#include <adapter_iptc/matchs/conntrack_match.h>
#include <adapter_iptc/matchs/hashlimit_match.h>
#include <adapter_iptc/matchs/mark_match.h>
#include <adapter_iptc/matchs/tcp_match.h>
#include <adapter_iptc/matchs/udp_match.h>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <linux/in.h>
#include <linux/netfilter/nf_conntrack_common.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_conntrack.h>
#include <linux/netfilter/xt_hashlimit.h>
#include <linux/netfilter/xt_mark.h>
#include <linux/netfilter/xt_tcpudp.h>
#include <string>
#include <vector>

namespace {

constexpr uint8_t ZERO_FILL_BYTE = 0;

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

std::vector<unsigned char> writeToZeroedBuffer(const TemplateMatch& match)
{
    std::vector<unsigned char> buffer(match.size(), ZERO_FILL_BYTE);
    match.write(buffer.data());
    return buffer;
}

} // namespace

TEST(MarkMatchTest, SizeCoversMatchHeaderAndMarkInfo)
{
    const MarkMatch match(SAMPLE_MARK);

    EXPECT_EQ(match.size(), XT_ALIGN(sizeof(xt_entry_match) + sizeof(xt_mark_mtinfo1)));
}

TEST(MarkMatchTest, WriteFillsHeaderWithMarkNameRevisionAndSize)
{
    const MarkMatch match(SAMPLE_MARK);
    const auto buffer = writeToZeroedBuffer(match);
    const xt_entry_match* header = headerOf(buffer);

    EXPECT_STREQ(header->u.user.name, MARK_NAME);
    EXPECT_EQ(header->u.user.revision, MARK_REVISION);
    EXPECT_EQ(static_cast<std::size_t>(header->u.user.match_size), match.size());
}

TEST(MarkMatchTest, WriteStoresMarkAndExplicitMask)
{
    const MarkMatch match(SAMPLE_MARK, SAMPLE_MARK_MASK);
    const auto buffer = writeToZeroedBuffer(match);
    const auto* info = reinterpret_cast<const xt_mark_mtinfo1*>(headerOf(buffer)->data);

    EXPECT_EQ(info->mark, SAMPLE_MARK);
    EXPECT_EQ(info->mask, SAMPLE_MARK_MASK);
}

TEST(MarkMatchTest, DefaultMaskCoversAllMarkBits)
{
    const MarkMatch match(SAMPLE_MARK);
    const auto buffer = writeToZeroedBuffer(match);
    const auto* info = reinterpret_cast<const xt_mark_mtinfo1*>(headerOf(buffer)->data);

    EXPECT_EQ(info->mask, FULL_MARK_MASK);
}

TEST(MarkMatchTest, DeclaresNoProtocol)
{
    const MarkMatch match(SAMPLE_MARK);

    EXPECT_EQ(match.protocol(), NO_PROTOCOL);
}

TEST(TcpMatchTest, SizeCoversMatchHeaderAndPortInfo)
{
    const TcpMatch match(SAMPLE_PORT);

    EXPECT_EQ(match.size(), XT_ALIGN(sizeof(xt_entry_match) + sizeof(xt_tcp)));
}

TEST(TcpMatchTest, WriteFillsHeaderWithTcpNameAndRevisionZero)
{
    const TcpMatch match(SAMPLE_PORT);
    const auto buffer = writeToZeroedBuffer(match);
    const xt_entry_match* header = headerOf(buffer);

    EXPECT_STREQ(header->u.user.name, TCP_NAME);
    EXPECT_EQ(header->u.user.revision, PORT_MATCH_REVISION);
    EXPECT_EQ(static_cast<std::size_t>(header->u.user.match_size), match.size());
}

TEST(TcpMatchTest, SinglePortSetsBothDestinationBoundsToThePort)
{
    const TcpMatch match(SAMPLE_PORT);
    const auto buffer = writeToZeroedBuffer(match);
    const auto* info = reinterpret_cast<const xt_tcp*>(headerOf(buffer)->data);

    EXPECT_EQ(info->dpts[0], SAMPLE_PORT);
    EXPECT_EQ(info->dpts[1], SAMPLE_PORT);
}

TEST(TcpMatchTest, PortRangeSetsDestinationMinAndMax)
{
    const TcpMatch match(SAMPLE_PORT_RANGE_MIN, SAMPLE_PORT_RANGE_MAX);
    const auto buffer = writeToZeroedBuffer(match);
    const auto* info = reinterpret_cast<const xt_tcp*>(headerOf(buffer)->data);

    EXPECT_EQ(info->dpts[0], SAMPLE_PORT_RANGE_MIN);
    EXPECT_EQ(info->dpts[1], SAMPLE_PORT_RANGE_MAX);
}

TEST(TcpMatchTest, SourcePortsSpanFullRange)
{
    const TcpMatch match(SAMPLE_PORT);
    const auto buffer = writeToZeroedBuffer(match);
    const auto* info = reinterpret_cast<const xt_tcp*>(headerOf(buffer)->data);

    EXPECT_EQ(info->spts[0], FULL_PORT_RANGE_MIN);
    EXPECT_EQ(info->spts[1], FULL_PORT_RANGE_MAX);
}

TEST(TcpMatchTest, DeclaresTcpProtocol)
{
    const TcpMatch match(SAMPLE_PORT);

    EXPECT_EQ(match.protocol(), IPPROTO_TCP);
}

TEST(UdpMatchTest, SizeCoversMatchHeaderAndPortInfo)
{
    const UdpMatch match(SAMPLE_PORT);

    EXPECT_EQ(match.size(), XT_ALIGN(sizeof(xt_entry_match) + sizeof(xt_udp)));
}

TEST(UdpMatchTest, WriteFillsHeaderWithUdpNameAndRevisionZero)
{
    const UdpMatch match(SAMPLE_PORT);
    const auto buffer = writeToZeroedBuffer(match);
    const xt_entry_match* header = headerOf(buffer);

    EXPECT_STREQ(header->u.user.name, UDP_NAME);
    EXPECT_EQ(header->u.user.revision, PORT_MATCH_REVISION);
    EXPECT_EQ(static_cast<std::size_t>(header->u.user.match_size), match.size());
}

TEST(UdpMatchTest, PortRangeSetsDestinationMinAndMax)
{
    const UdpMatch match(SAMPLE_PORT_RANGE_MIN, SAMPLE_PORT_RANGE_MAX);
    const auto buffer = writeToZeroedBuffer(match);
    const auto* info = reinterpret_cast<const xt_udp*>(headerOf(buffer)->data);

    EXPECT_EQ(info->dpts[0], SAMPLE_PORT_RANGE_MIN);
    EXPECT_EQ(info->dpts[1], SAMPLE_PORT_RANGE_MAX);
}

TEST(UdpMatchTest, SourcePortsSpanFullRange)
{
    const UdpMatch match(SAMPLE_PORT);
    const auto buffer = writeToZeroedBuffer(match);
    const auto* info = reinterpret_cast<const xt_udp*>(headerOf(buffer)->data);

    EXPECT_EQ(info->spts[0], FULL_PORT_RANGE_MIN);
    EXPECT_EQ(info->spts[1], FULL_PORT_RANGE_MAX);
}

TEST(UdpMatchTest, DeclaresUdpProtocol)
{
    const UdpMatch match(SAMPLE_PORT);

    EXPECT_EQ(match.protocol(), IPPROTO_UDP);
}

TEST(ConntrackMatchTest, SizeCoversMatchHeaderAndConntrackInfo)
{
    const ConntrackMatch match(ConntrackMatch::State::NEW);

    EXPECT_EQ(match.size(), XT_ALIGN(sizeof(xt_entry_match) + sizeof(xt_conntrack_mtinfo3)));
}

TEST(ConntrackMatchTest, WriteFillsHeaderWithConntrackNameAndRevisionThree)
{
    const ConntrackMatch match(ConntrackMatch::State::ESTABLISHED_RELATED);
    const auto buffer = writeToZeroedBuffer(match);
    const xt_entry_match* header = headerOf(buffer);

    EXPECT_STREQ(header->u.user.name, CONNTRACK_NAME);
    EXPECT_EQ(header->u.user.revision, CONNTRACK_REVISION);
    EXPECT_EQ(static_cast<std::size_t>(header->u.user.match_size), match.size());
}

TEST(ConntrackMatchTest, NewStateMatchesNewBitWithDirectionFlag)
{
    const ConntrackMatch match(ConntrackMatch::State::NEW);
    const auto buffer = writeToZeroedBuffer(match);
    const auto* info = reinterpret_cast<const xt_conntrack_mtinfo3*>(headerOf(buffer)->data);

    EXPECT_EQ(info->match_flags, XT_CONNTRACK_STATE | XT_CONNTRACK_DIRECTION);
    EXPECT_EQ(info->state_mask, NF_CT_STATE_BIT(IP_CT_NEW));
}

TEST(ConntrackMatchTest, EstablishedRelatedStateMatchesEstablishedAndRelatedBits)
{
    const ConntrackMatch match(ConntrackMatch::State::ESTABLISHED_RELATED);
    const auto buffer = writeToZeroedBuffer(match);
    const auto* info = reinterpret_cast<const xt_conntrack_mtinfo3*>(headerOf(buffer)->data);

    EXPECT_EQ(info->match_flags, XT_CONNTRACK_STATE);
    EXPECT_EQ(info->state_mask,
              NF_CT_STATE_BIT(IP_CT_ESTABLISHED) | NF_CT_STATE_BIT(IP_CT_RELATED));
}

TEST(ConntrackMatchTest, InvalidStateMatchesInvalidBit)
{
    const ConntrackMatch match(ConntrackMatch::State::INVALID);
    const auto buffer = writeToZeroedBuffer(match);
    const auto* info = reinterpret_cast<const xt_conntrack_mtinfo3*>(headerOf(buffer)->data);

    EXPECT_EQ(info->match_flags, XT_CONNTRACK_STATE);
    EXPECT_EQ(info->state_mask, NF_CT_STATE_INVALID_BIT);
}

TEST(HashlimitMatchTest, SizeCoversMatchHeaderAndHashlimitInfo)
{
    const HashlimitMatch match(SAMPLE_HASHLIMIT_NAME, SAMPLE_HASHLIMIT_AVERAGE,
                               SAMPLE_HASHLIMIT_BURST, SAMPLE_HASHLIMIT_EXPIRE);

    EXPECT_EQ(match.size(), XT_ALIGN(sizeof(xt_entry_match) + sizeof(xt_hashlimit_mtinfo1)));
}

TEST(HashlimitMatchTest, WriteFillsHeaderWithHashlimitNameAndRevisionOne)
{
    const HashlimitMatch match(SAMPLE_HASHLIMIT_NAME, SAMPLE_HASHLIMIT_AVERAGE,
                               SAMPLE_HASHLIMIT_BURST, SAMPLE_HASHLIMIT_EXPIRE);
    const auto buffer = writeToZeroedBuffer(match);
    const xt_entry_match* header = headerOf(buffer);

    EXPECT_STREQ(header->u.user.name, HASHLIMIT_NAME);
    EXPECT_EQ(header->u.user.revision, HASHLIMIT_REVISION);
    EXPECT_EQ(static_cast<std::size_t>(header->u.user.match_size), match.size());
}

TEST(HashlimitMatchTest, WriteFillsNamedRateConfigWithDefaults)
{
    const HashlimitMatch match(SAMPLE_HASHLIMIT_NAME, SAMPLE_HASHLIMIT_AVERAGE,
                               SAMPLE_HASHLIMIT_BURST, SAMPLE_HASHLIMIT_EXPIRE);
    const auto buffer = writeToZeroedBuffer(match);
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
    const HashlimitMatch match(SAMPLE_HASHLIMIT_NAME, SAMPLE_HASHLIMIT_AVERAGE,
                               SAMPLE_HASHLIMIT_BURST, SAMPLE_HASHLIMIT_EXPIRE,
                               DEFAULT_GARBAGE_COLLECTION_INTERVAL, DEFAULT_HOST_MASK,
                               DEFAULT_HOST_MASK, EXPLICIT_HASHLIMIT_MODE);
    const auto buffer = writeToZeroedBuffer(match);
    const auto* info = reinterpret_cast<const xt_hashlimit_mtinfo1*>(headerOf(buffer)->data);

    EXPECT_EQ(info->cfg.mode, EXPLICIT_HASHLIMIT_MODE);
}
