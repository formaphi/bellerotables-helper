#include <adapter_iptc/targets/nflog_target.h>
#include <adapter_iptc/targets/standard_target.h>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <linux/netfilter.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_NFLOG.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <vector>

namespace {

constexpr uint8_t ZERO_FILL_BYTE = 0;

constexpr int ACCEPT_VERDICT = -NF_ACCEPT - 1;
constexpr int DROP_VERDICT = -NF_DROP - 1;
constexpr int RETURN_VERDICT = XT_RETURN;
constexpr const char* EMPTY_TARGET_NAME = "";

constexpr const char* NFLOG_NAME = "NFLOG";
constexpr uint16_t SAMPLE_NFLOG_GROUP = 7;
constexpr const char* SAMPLE_NFLOG_PREFIX = "bb-drop";

std::vector<unsigned char> writeToZeroedBuffer(const ITargetStrategy& target)
{
    std::vector<unsigned char> buffer(target.size(), ZERO_FILL_BYTE);
    target.write(buffer.data());
    return buffer;
}

int writtenVerdict(const StandardTarget& target)
{
    const auto buffer = writeToZeroedBuffer(target);
    return reinterpret_cast<const xt_standard_target*>(buffer.data())->verdict;
}

} // namespace

TEST(StandardTargetTest, SizeCoversStandardTargetStruct)
{
    const StandardTarget target(StandardTarget::Verdict::ACCEPT);

    EXPECT_EQ(target.size(), XT_ALIGN(sizeof(xt_standard_target)));
}

TEST(StandardTargetTest, AcceptWritesNegativeAcceptVerdict)
{
    const StandardTarget target(StandardTarget::Verdict::ACCEPT);

    EXPECT_EQ(writtenVerdict(target), ACCEPT_VERDICT);
}

TEST(StandardTargetTest, DropWritesNegativeDropVerdict)
{
    const StandardTarget target(StandardTarget::Verdict::DROP);

    EXPECT_EQ(writtenVerdict(target), DROP_VERDICT);
}

TEST(StandardTargetTest, ReturnWritesXtReturnVerdict)
{
    const StandardTarget target(StandardTarget::Verdict::RETURN);

    EXPECT_EQ(writtenVerdict(target), RETURN_VERDICT);
}

TEST(StandardTargetTest, HasEmptyTargetName)
{
    const StandardTarget target(StandardTarget::Verdict::ACCEPT);
    const auto buffer = writeToZeroedBuffer(target);
    const auto* standard = reinterpret_cast<const xt_standard_target*>(buffer.data());

    EXPECT_STREQ(standard->target.u.user.name, EMPTY_TARGET_NAME);
}

TEST(StandardTargetTest, RecordsItsSizeInTargetHeader)
{
    const StandardTarget target(StandardTarget::Verdict::DROP);
    const auto buffer = writeToZeroedBuffer(target);
    const auto* standard = reinterpret_cast<const xt_standard_target*>(buffer.data());

    EXPECT_EQ(static_cast<std::size_t>(standard->target.u.user.target_size), target.size());
}

TEST(NflogTargetTest, SizeCoversExtensionTargetHeaderAndNflogInfo)
{
    const NflogTarget target(SAMPLE_NFLOG_GROUP, SAMPLE_NFLOG_PREFIX);

    EXPECT_EQ(target.size(), XT_ALIGN(sizeof(xt_entry_target) + sizeof(xt_nflog_info)));
}

TEST(NflogTargetTest, WriteNamesTargetNflogAndRecordsItsSize)
{
    const NflogTarget target(SAMPLE_NFLOG_GROUP, SAMPLE_NFLOG_PREFIX);
    const auto buffer = writeToZeroedBuffer(target);
    const auto* header = reinterpret_cast<const xt_entry_target*>(buffer.data());

    EXPECT_STREQ(header->u.user.name, NFLOG_NAME);
    EXPECT_EQ(static_cast<std::size_t>(header->u.user.target_size), target.size());
}

TEST(NflogTargetTest, WriteFillsGroupAndPrefix)
{
    const NflogTarget target(SAMPLE_NFLOG_GROUP, SAMPLE_NFLOG_PREFIX);
    const auto buffer = writeToZeroedBuffer(target);
    const auto* info = reinterpret_cast<const xt_nflog_info*>(
        reinterpret_cast<const xt_entry_target*>(buffer.data())->data);

    EXPECT_EQ(info->group, SAMPLE_NFLOG_GROUP);
    EXPECT_STREQ(info->prefix, SAMPLE_NFLOG_PREFIX);
}
