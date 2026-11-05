#include "adapter_socket/peer_connection/peer_credentials.h"

#include "parameterized_case.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <sys/socket.h>
#include <sys/types.h>
#include <vector>

constexpr uid_t SAMPLE_UID = 1000;
constexpr gid_t SAMPLE_GID = 1001;
constexpr pid_t SAMPLE_PID = 4242;
constexpr uint32_t SAMPLE_USER_ID = 1000;
constexpr uint32_t SAMPLE_GROUP_ID = 1001;
constexpr uint32_t SAMPLE_PROCESS_ID = 4242;
constexpr uid_t LARGEST_VALID_UID = std::numeric_limits<uid_t>::max() - 1;
constexpr gid_t LARGEST_VALID_GID = std::numeric_limits<gid_t>::max() - 2;
constexpr pid_t LARGEST_LINUX_PID = 4194304;
constexpr uint32_t LARGEST_VALID_USER_ID = 4294967294U;
constexpr uint32_t LARGEST_VALID_GROUP_ID = 4294967293U;
constexpr uint32_t LARGEST_LINUX_PROCESS_ID = 4194304;
constexpr uid_t INVALID_UID = static_cast<uid_t>(-1);
constexpr gid_t INVALID_GID = static_cast<gid_t>(-1);
constexpr pid_t NO_PEER_PID = 0;
constexpr uint32_t INVALID_ID = std::numeric_limits<uint32_t>::max();
constexpr uint32_t NO_PEER_PROCESS_ID = 0;

struct UcredCase
{
    uid_t uid;
    gid_t gid;
    pid_t pid;
    uint32_t expectedUserId;
    uint32_t expectedGroupId;
    uint32_t expectedProcessId;
    const char* name;
};

/* LEARN: every case uses three different values, so a uid/gid/pid mix-up always shows. A real
   socket cannot catch it when uid == gid (the sandbox runs as 0/0). The invalid case is what the
   kernel reports for a socket without a peer; it must stay 0xFFFFFFFF and never become 0 (root).
*/
const std::vector<UcredCase> UCRED_MAPPINGS{
    {SAMPLE_UID, SAMPLE_GID, SAMPLE_PID, SAMPLE_USER_ID, SAMPLE_GROUP_ID, SAMPLE_PROCESS_ID,
     "DistinctIds"},
    {LARGEST_VALID_UID, LARGEST_VALID_GID, LARGEST_LINUX_PID, LARGEST_VALID_USER_ID,
     LARGEST_VALID_GROUP_ID, LARGEST_LINUX_PROCESS_ID, "LargestIdsWithoutNarrowing"},
    {INVALID_UID, INVALID_GID, NO_PEER_PID, INVALID_ID, INVALID_ID, NO_PEER_PROCESS_ID,
     "InvalidIdsStayDistinctFromRoot"},
};

class PeerCredentialsTest : public ::testing::TestWithParam<UcredCase>
{
};

TEST_P(PeerCredentialsTest, FromUcredMapsUidGidAndPid)
{
    ucred credentials{};
    credentials.uid = GetParam().uid;
    credentials.gid = GetParam().gid;
    credentials.pid = GetParam().pid;

    const adapter_socket::PeerCredentials peer =
        adapter_socket::PeerCredentials::fromUcred(credentials);

    EXPECT_EQ(peer.userId, GetParam().expectedUserId);
    EXPECT_EQ(peer.groupId, GetParam().expectedGroupId);
    EXPECT_EQ(peer.processId, GetParam().expectedProcessId);
}

INSTANTIATE_TEST_SUITE_P(Ucred, PeerCredentialsTest, ::testing::ValuesIn(UCRED_MAPPINGS),
                         caseName<UcredCase>);
