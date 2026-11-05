#include <adapter_socket/socket_syscalls/ucred_conversion.h>

#include <adapter_socket/peer_connection/peer_credentials.h>

#include <gtest/gtest.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <cstdint>

constexpr uid_t SAMPLE_UID = 1000;
constexpr gid_t SAMPLE_GID = 1001;
constexpr pid_t SAMPLE_PID = 4242;
constexpr uint32_t SAMPLE_USER_ID = 1000;
constexpr uint32_t SAMPLE_GROUP_ID = 1001;
constexpr uint32_t SAMPLE_PROCESS_ID = 4242;

TEST(UcredConversionTest, MapsUidGidAndPid)
{
    ucred credentials{};
    credentials.uid = SAMPLE_UID;
    credentials.gid = SAMPLE_GID;
    credentials.pid = SAMPLE_PID;

    const adapter_socket::PeerCredentials peer =
        adapter_socket::peerCredentialsFromUcred(credentials);

    EXPECT_EQ(peer.userId, SAMPLE_USER_ID);
    EXPECT_EQ(peer.groupId, SAMPLE_GROUP_ID);
    EXPECT_EQ(peer.processId, SAMPLE_PROCESS_ID);
}
