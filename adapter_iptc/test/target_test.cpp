#include "adapter_iptc/i_target_strategy.h"
#include "adapter_iptc/targets/nflog_target.h"
#include "adapter_iptc/targets/standard_target.h"
#include "parameterized_case.h"

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <linux/netfilter.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_NFLOG.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <memory>
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

const xt_entry_target* headerOf(const std::vector<unsigned char>& buffer)
{
    return reinterpret_cast<const xt_entry_target*>(buffer.data());
}

} // namespace

/* LEARN: every target starts with the same xt_entry_target header (name, size). A standard
   target (ACCEPT/DROP/RETURN) has an empty name and a verdict as payload; an extension target
   (NFLOG) is named and carries its own payload.
*/
struct TargetHeaderCase
{
    std::shared_ptr<ITargetStrategy> (*makeTarget)();
    const char* expectedName;
    std::size_t expectedSize;
    const char* name;
};

const std::vector<TargetHeaderCase> TARGET_HEADER_CASES{
    {[]() -> std::shared_ptr<ITargetStrategy> {
         return std::make_shared<StandardTarget>(StandardTarget::Verdict::ACCEPT);
     },
     EMPTY_TARGET_NAME, XT_ALIGN(sizeof(xt_standard_target)), "StandardAccept"},
    {[]() -> std::shared_ptr<ITargetStrategy> {
         return std::make_shared<StandardTarget>(StandardTarget::Verdict::DROP);
     },
     EMPTY_TARGET_NAME, XT_ALIGN(sizeof(xt_standard_target)), "StandardDrop"},
    {[]() -> std::shared_ptr<ITargetStrategy> {
         return std::make_shared<NflogTarget>(SAMPLE_NFLOG_GROUP, SAMPLE_NFLOG_PREFIX);
     },
     NFLOG_NAME, XT_ALIGN(sizeof(xt_entry_target) + sizeof(xt_nflog_info)), "Nflog"},
};

class TargetHeaderTest : public ::testing::TestWithParam<TargetHeaderCase>
{
};

TEST_P(TargetHeaderTest, SizeCoversHeaderAndPayload)
{
    const std::shared_ptr<ITargetStrategy> target = GetParam().makeTarget();

    EXPECT_EQ(target->size(), GetParam().expectedSize);
}

TEST_P(TargetHeaderTest, WriteRecordsNameAndSizeInTheHeader)
{
    const std::shared_ptr<ITargetStrategy> target = GetParam().makeTarget();
    const auto buffer = writeToZeroedBuffer(*target);

    EXPECT_STREQ(headerOf(buffer)->u.user.name, GetParam().expectedName);
    EXPECT_EQ(static_cast<std::size_t>(headerOf(buffer)->u.user.target_size), target->size());
}

INSTANTIATE_TEST_SUITE_P(Target, TargetHeaderTest, ::testing::ValuesIn(TARGET_HEADER_CASES),
                         caseName<TargetHeaderCase>);

struct VerdictCase
{
    StandardTarget::Verdict verdict;
    int expectedKernelVerdict;
    const char* name;
};

/* LEARN: the kernel encodes ACCEPT/DROP as -(verdict) - 1 so they never collide with a jump
   offset (>= 0); RETURN has its own negative constant.
*/
const std::vector<VerdictCase> VERDICT_CASES{
    {StandardTarget::Verdict::ACCEPT, ACCEPT_VERDICT, "Accept"},
    {StandardTarget::Verdict::DROP, DROP_VERDICT, "Drop"},
    {StandardTarget::Verdict::RETURN, RETURN_VERDICT, "Return"},
};

class StandardTargetVerdictTest : public ::testing::TestWithParam<VerdictCase>
{
};

TEST_P(StandardTargetVerdictTest, WritesTheKernelVerdict)
{
    const StandardTarget target(GetParam().verdict);
    const auto buffer = writeToZeroedBuffer(target);
    const auto* standard = reinterpret_cast<const xt_standard_target*>(buffer.data());

    EXPECT_EQ(standard->verdict, GetParam().expectedKernelVerdict);
}

INSTANTIATE_TEST_SUITE_P(StandardTarget, StandardTargetVerdictTest,
                         ::testing::ValuesIn(VERDICT_CASES), caseName<VerdictCase>);
