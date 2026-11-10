#include "adapter_socket/socket_endpoint/socket_endpoint.h"

#include "adapter_socket/deadline/interface_deadline.h"
#include "adapter_socket/exceptions/socket_exception.h"
#include "adapter_socket/file_descriptor/interface_file_descriptor.h"
#include "adapter_socket/mock/mock_deadline.h"
#include "adapter_socket/mock/mock_deadline_factory.h"
#include "adapter_socket/mock/mock_file_descriptor.h"
#include "adapter_socket/mock/mock_peer_policy.h"
#include "adapter_socket/mock/mock_socket_activation.h"
#include "adapter_socket/mock/mock_socket_connection.h"
#include "adapter_socket/mock/mock_socket_listener.h"
#include "adapter_socket/mock/mock_socket_listener_factory.h"
#include "adapter_socket/peer_connection/peer_credentials.h"
#include "adapter_socket/socket_endpoint/socket_timeouts_dto.h"
#include "adapter_socket/socket_listener/accepted_connection_dto.h"

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <system_error>
#include <utility>

using ::testing::_;
using ::testing::ByMove;
using ::testing::Field;
using ::testing::Return;
using ::testing::Throw;
using ::testing::Truly;

using adapter_socket::AcceptedConnectionDto;
using adapter_socket::ActivationException;
using adapter_socket::IDeadline;
using adapter_socket::IFileDescriptor;
using adapter_socket::IFramedConnection;
using adapter_socket::IPeerPolicy;
using adapter_socket::ISocketListener;
using adapter_socket::MockDeadline;
using adapter_socket::MockDeadlineFactory;
using adapter_socket::MockFileDescriptor;
using adapter_socket::MockPeerPolicy;
using adapter_socket::MockSocketActivation;
using adapter_socket::MockSocketConnection;
using adapter_socket::MockSocketListener;
using adapter_socket::MockSocketListenerFactory;
using adapter_socket::PeerCredentials;
using adapter_socket::SocketEndpoint;
using adapter_socket::SocketTimeoutsDto;

constexpr uint32_t AGENT_USER_ID = 900;
constexpr uint32_t AGENT_GROUP_ID = 900;
constexpr uint32_t AGENT_PROCESS_ID = 4242;
constexpr uint32_t STRANGER_USER_ID = 1001;
constexpr uint32_t STRANGER_GROUP_ID = 1001;
constexpr uint32_t STRANGER_PROCESS_ID = 4343;
constexpr PeerCredentials AGENT_PEER{AGENT_USER_ID, AGENT_GROUP_ID, AGENT_PROCESS_ID};
constexpr PeerCredentials STRANGER_PEER{STRANGER_USER_ID, STRANGER_GROUP_ID, STRANGER_PROCESS_ID};
constexpr std::chrono::milliseconds ACCEPT_TIMEOUT(250);
constexpr std::chrono::milliseconds CONNECTION_TIMEOUT(750);
constexpr const char* ACTIVATION_FAILURE_MESSAGE = "expected exactly one activated socket";
constexpr int ACCEPT_FAILURE_ERRNO = ETIMEDOUT;
constexpr const char* ACCEPT_OPERATION = "accept";

class SocketEndpointTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        std::unique_ptr<MockPeerPolicy> peerPolicy = std::make_unique<MockPeerPolicy>();
        m_peerPolicy = peerPolicy.get();
        m_ownedPeerPolicy = std::move(peerPolicy);

        std::unique_ptr<MockSocketListener> listener = std::make_unique<MockSocketListener>();
        m_listener = listener.get();
        m_ownedListener = std::move(listener);

        EXPECT_CALL(*m_deadlineFactory, startingNow(_))
            .WillRepeatedly([](std::chrono::milliseconds) -> std::unique_ptr<IDeadline> {
                return std::make_unique<MockDeadline>();
            });
    }

    std::unique_ptr<SocketEndpoint> buildEndpoint()
    {
        return std::make_unique<SocketEndpoint>(
            m_activation, m_listenerFactory, std::move(m_ownedPeerPolicy), m_deadlineFactory,
            SocketTimeoutsDto::create(ACCEPT_TIMEOUT, CONNECTION_TIMEOUT));
    }

    /* LEARN: the endpoint only accepts and authorizes. It never reads or writes a byte: a refused
       peer gets nothing, and an allowed peer's bytes belong to whoever reads the returned
       IFramedConnection.
    */
    static AcceptedConnectionDto acceptedFrom(const PeerCredentials& peer)
    {
        std::unique_ptr<MockSocketConnection> connection = std::make_unique<MockSocketConnection>();
        EXPECT_CALL(*connection, readExactly(_, _)).Times(0);
        EXPECT_CALL(*connection, writeAll(_, _)).Times(0);
        return AcceptedConnectionDto{std::move(connection), peer};
    }

    void expectActivatedFdReachesFactory()
    {
        std::unique_ptr<IFileDescriptor> listeningFd = std::make_unique<MockFileDescriptor>();
        m_activatedFd = listeningFd.get();
        EXPECT_CALL(*m_activation, takeListeningFd())
            .WillOnce(Return(ByMove(std::move(listeningFd))));
        EXPECT_CALL(*m_listenerFactory, create(matchesActivatedFd()))
            .WillOnce(Return(ByMove(std::move(m_ownedListener))));
    }

    testing::Matcher<std::unique_ptr<IFileDescriptor>> matchesActivatedFd() const
    {
        const IFileDescriptor* const expected = m_activatedFd;
        return Truly([expected](const std::unique_ptr<IFileDescriptor>& passed) {
            return passed.get() == expected;
        });
    }

    std::shared_ptr<MockSocketActivation> m_activation = std::make_shared<MockSocketActivation>();
    std::shared_ptr<MockSocketListenerFactory> m_listenerFactory =
        std::make_shared<MockSocketListenerFactory>();
    std::shared_ptr<MockDeadlineFactory> m_deadlineFactory =
        std::make_shared<MockDeadlineFactory>();
    std::unique_ptr<IPeerPolicy> m_ownedPeerPolicy;
    std::unique_ptr<ISocketListener> m_ownedListener;
    MockPeerPolicy* m_peerPolicy = nullptr;
    MockSocketListener* m_listener = nullptr;
    const IFileDescriptor* m_activatedFd = nullptr;
};

TEST_F(SocketEndpointTest, PassesActivatedFdToListenerFactory)
{
    expectActivatedFdReachesFactory();
    EXPECT_CALL(*m_listener, acceptOne(_)).Times(0);

    const std::unique_ptr<SocketEndpoint> endpoint = buildEndpoint();

    EXPECT_NE(endpoint.get(), nullptr);
}

TEST_F(SocketEndpointTest, ReturnsEmptyWhenPeerIsRefused)
{
    expectActivatedFdReachesFactory();
    EXPECT_CALL(*m_listener, acceptOne(_)).WillOnce(Return(ByMove(acceptedFrom(STRANGER_PEER))));
    EXPECT_CALL(*m_peerPolicy, isAllowed(Field(&PeerCredentials::userId, STRANGER_USER_ID)))
        .WillOnce(Return(false));

    const std::unique_ptr<SocketEndpoint> endpoint = buildEndpoint();

    EXPECT_FALSE(endpoint->acceptAuthorized().has_value());
}

TEST_F(SocketEndpointTest, ReturnsFramedConnectionWhenPeerIsAllowed)
{
    expectActivatedFdReachesFactory();
    EXPECT_CALL(*m_listener, acceptOne(_)).WillOnce(Return(ByMove(acceptedFrom(AGENT_PEER))));
    EXPECT_CALL(*m_peerPolicy, isAllowed(Field(&PeerCredentials::userId, AGENT_USER_ID)))
        .WillOnce(Return(true));

    const std::unique_ptr<SocketEndpoint> endpoint = buildEndpoint();
    const std::optional<std::unique_ptr<IFramedConnection>> framed = endpoint->acceptAuthorized();

    ASSERT_TRUE(framed.has_value());
    EXPECT_NE(framed.value().get(), nullptr);
}

TEST_F(SocketEndpointTest, AcceptDeadlineUsesConfiguredAcceptTimeout)
{
    expectActivatedFdReachesFactory();
    EXPECT_CALL(*m_deadlineFactory, startingNow(ACCEPT_TIMEOUT))
        .Times(1)
        .WillOnce([](std::chrono::milliseconds) -> std::unique_ptr<IDeadline> {
            return std::make_unique<MockDeadline>();
        });
    EXPECT_CALL(*m_listener, acceptOne(_)).WillOnce(Return(ByMove(acceptedFrom(STRANGER_PEER))));
    EXPECT_CALL(*m_peerPolicy, isAllowed(_)).WillOnce(Return(false));

    const std::unique_ptr<SocketEndpoint> endpoint = buildEndpoint();

    EXPECT_FALSE(endpoint->acceptAuthorized().has_value());
}

TEST_F(SocketEndpointTest, ActivationFailurePropagatesAndListenerIsNeverCreated)
{
    EXPECT_CALL(*m_activation, takeListeningFd())
        .WillOnce(Throw(ActivationException(ACTIVATION_FAILURE_MESSAGE)));
    EXPECT_CALL(*m_listenerFactory, create(_)).Times(0);

    EXPECT_THROW(buildEndpoint(), ActivationException);
}

TEST_F(SocketEndpointTest, AcceptFailurePropagatesFromAcceptAuthorized)
{
    expectActivatedFdReachesFactory();
    EXPECT_CALL(*m_listener, acceptOne(_))
        .WillOnce(Throw(
            std::system_error(ACCEPT_FAILURE_ERRNO, std::generic_category(), ACCEPT_OPERATION)));
    EXPECT_CALL(*m_peerPolicy, isAllowed(_)).Times(0);

    const std::unique_ptr<SocketEndpoint> endpoint = buildEndpoint();

    EXPECT_THROW(endpoint->acceptAuthorized(), std::system_error);
}
