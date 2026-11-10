#include "adapter_socket/socket_connection/unix_socket_connection.h"

#include "adapter_socket/deadline/interface_deadline.h"
#include "adapter_socket/deadline/interface_deadline_factory.h"
#include "adapter_socket/deadline/steady_deadline_factory.h"
#include "adapter_socket/descriptor_syscalls/interface_descriptor_syscalls.h"
#include "adapter_socket/descriptor_syscalls/posix_descriptor_syscalls.h"
#include "adapter_socket/exceptions/socket_exception.h"
#include "adapter_socket/file_descriptor/file_descriptor.h"
#include "adapter_socket/file_descriptor/interface_file_descriptor.h"
#include "adapter_socket/mock/mock_deadline.h"
#include "adapter_socket/mock/mock_file_descriptor.h"
#include "adapter_socket/mock/mock_socket_syscalls.h"
#include "adapter_socket/socket_syscalls/interface_socket_syscalls.h"
#include "adapter_socket/socket_syscalls/posix_socket_syscalls.h"
#include "parameterized_case.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <functional>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <span>
#include <sys/socket.h>
#include <sys/types.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

using adapter_socket::FileDescriptor;
using adapter_socket::IDeadline;
using adapter_socket::IDeadlineFactory;
using adapter_socket::IDescriptorSyscalls;
using adapter_socket::IFileDescriptor;
using adapter_socket::ISocketSyscalls;
using adapter_socket::MockDeadline;
using adapter_socket::MockFileDescriptor;
using adapter_socket::MockSocketSyscalls;
using adapter_socket::PosixDescriptorSyscalls;
using adapter_socket::PosixSocketSyscalls;
using adapter_socket::Readiness;
using adapter_socket::SteadyDeadlineFactory;
using adapter_socket::SyscallResult;
using adapter_socket::UnixSocketConnection;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;

constexpr std::size_t LARGE_PAYLOAD_BYTES = 1048576;
constexpr std::size_t BLOCKED_WRITE_PAYLOAD_BYTES = 4194304;
constexpr uint8_t PAYLOAD_FILL_BYTE = 0x5A;
constexpr std::size_t FIVE_BYTES = 5;
constexpr std::size_t FOUR_BYTES = 4;
constexpr std::size_t THREE_BYTES = 3;
const std::vector<uint8_t> FIRST_CHUNK{1, 2, 3};
const std::vector<uint8_t> SECOND_CHUNK{4, 5};
const std::vector<uint8_t> BOTH_CHUNKS{1, 2, 3, 4, 5};
const std::vector<uint8_t> SHORT_CHUNK{7, 8};
const std::vector<uint8_t> WRITTEN_BYTES{10, 20, 30};
constexpr std::size_t SLOW_LORIS_BYTE_COUNT = 10;
constexpr std::size_t NO_BYTES_TRANSFERRED = 0;
constexpr std::chrono::milliseconds DEFAULT_TEST_TIMEOUT(5000);
constexpr std::chrono::milliseconds IDLE_READ_TIMEOUT(300);
constexpr std::chrono::milliseconds BLOCKED_WRITE_TIMEOUT(300);
constexpr std::chrono::milliseconds SLOW_LORIS_TIMEOUT(400);
constexpr std::chrono::milliseconds DEADLINE_UPPER_MARGIN(700);
constexpr std::size_t SINGLE_BYTE = 1;
constexpr ssize_t NO_PROGRESS = 0;
constexpr int SYSCALL_ERROR = -1;
constexpr int SYSCALL_SUCCESS = 0;
constexpr int NO_FLAGS = 0;
constexpr int DEFAULT_PROTOCOL = 0;
constexpr int FIRST_VALID_FD = 0;
constexpr int INVALID_FD = -1;
constexpr std::size_t LOCAL_END = 0;
constexpr std::size_t PEER_END = 1;
constexpr uint8_t SLOW_LORIS_BYTE = 0x2A;
constexpr std::chrono::milliseconds SLOW_LORIS_INTERVAL(150);
constexpr int FAKE_CONNECTED_FD = 4242;
constexpr std::size_t TWO_BYTES = 2;
constexpr int NO_ERROR_CAUGHT = 0;

class StopAndJoin final
{
public:
    StopAndJoin(std::atomic<bool>& stopRequested, std::thread& worker)
        : m_stopRequested(stopRequested)
        , m_worker(worker)
    {
    }

    ~StopAndJoin()
    {
        m_stopRequested = true;
        if (m_worker.joinable())
        {
            m_worker.join();
        }
    }

    StopAndJoin(const StopAndJoin&) = delete;
    StopAndJoin& operator=(const StopAndJoin&) = delete;

private:
    std::atomic<bool>& m_stopRequested;
    std::thread& m_worker;
};

class UnixSocketConnectionTest : public ::testing::Test
{
protected:
    static std::unique_ptr<IDeadline> expiredDeadline()
    {
        std::unique_ptr<MockDeadline> deadline = std::make_unique<MockDeadline>();
        EXPECT_CALL(*deadline, hasExpired()).WillRepeatedly(Return(true));
        return deadline;
    }

    static bool isFdClosed(int fd)
    {
        const int descriptorFlags = ::fcntl(fd, F_GETFD);
        const int savedErrno = errno;
        return descriptorFlags == SYSCALL_ERROR && savedErrno == EBADF;
    }

    static void sendBytes(int fd, const std::vector<uint8_t>& bytes)
    {
        std::size_t sent = NO_BYTES_TRANSFERRED;
        while (sent < bytes.size())
        {
            const std::size_t remainingBytes = bytes.size() - sent;
            const uint8_t* const source = bytes.data() + sent;
            const ssize_t result = ::send(fd, source, remainingBytes, MSG_NOSIGNAL);
            if (result <= NO_PROGRESS)
            {
                const int savedErrno = errno;
                throw std::system_error(savedErrno, std::generic_category(), "send in test");
            }
            sent += static_cast<std::size_t>(result);
        }
    }

    static std::vector<uint8_t> receiveBytes(int fd, std::size_t byteCount)
    {
        std::vector<uint8_t> bytes(byteCount);
        std::size_t received = NO_BYTES_TRANSFERRED;
        while (received < byteCount)
        {
            const std::size_t remainingBytes = byteCount - received;
            uint8_t* const destination = bytes.data() + received;
            const ssize_t result = ::recv(fd, destination, remainingBytes, NO_FLAGS);
            if (result <= NO_PROGRESS)
            {
                const int savedErrno = errno;
                throw std::system_error(savedErrno, std::generic_category(), "recv in test");
            }
            received += static_cast<std::size_t>(result);
        }
        return bytes;
    }

    static void trickleBytes(int fd, const std::atomic<bool>& stopRequested)
    {
        std::size_t sentCount = NO_BYTES_TRANSFERRED;
        while (true)
        {
            const bool shouldTrickle = sentCount < SLOW_LORIS_BYTE_COUNT && !stopRequested.load();
            if (!shouldTrickle)
            {
                return;
            }

            const std::vector<uint8_t> oneByte{SLOW_LORIS_BYTE};
            const ssize_t sendResult = ::send(fd, oneByte.data(), oneByte.size(), MSG_NOSIGNAL);
            if (sendResult == SYSCALL_ERROR)
            {
                return;
            }
            std::this_thread::sleep_for(SLOW_LORIS_INTERVAL);
            ++sentCount;
        }
    }

    void SetUp() override
    {
        std::array<int, 2> fds{};
        const int pairResult = ::socketpair(AF_UNIX, SOCK_STREAM, DEFAULT_PROTOCOL, fds.data());
        if (pairResult != SYSCALL_SUCCESS)
        {
            const int savedErrno = errno;
            throw std::system_error(savedErrno, std::generic_category(), "socketpair");
        }

        m_localFd = fds[LOCAL_END];
        m_peerFd = fds[PEER_END];
    }

    void TearDown() override
    {
        closePeer();
    }

    void closePeer()
    {
        if (m_peerFd >= FIRST_VALID_FD)
        {
            ::close(m_peerFd);
            m_peerFd = INVALID_FD;
        }
    }

    int m_localFd = INVALID_FD;
    int m_peerFd = INVALID_FD;
    std::unique_ptr<IDeadlineFactory> m_deadlines = std::make_unique<SteadyDeadlineFactory>();
    std::shared_ptr<IDescriptorSyscalls> m_descriptorSyscalls =
        std::make_shared<PosixDescriptorSyscalls>();
    std::shared_ptr<ISocketSyscalls> m_socketSyscalls = std::make_shared<PosixSocketSyscalls>();
};

TEST_F(UnixSocketConnectionTest, ReadExactlyAssemblesBytesWrittenInTwoSends)
{
    std::unique_ptr<IFileDescriptor> localFd =
        std::make_unique<FileDescriptor>(m_localFd, m_descriptorSyscalls);
    UnixSocketConnection connection(std::move(localFd), m_socketSyscalls);
    const std::unique_ptr<IDeadline> deadline = m_deadlines->startingNow(DEFAULT_TEST_TIMEOUT);

    sendBytes(m_peerFd, FIRST_CHUNK);
    sendBytes(m_peerFd, SECOND_CHUNK);

    EXPECT_EQ(connection.readExactly(FIVE_BYTES, *deadline), BOTH_CHUNKS);
}

TEST_F(UnixSocketConnectionTest, ReadExactlyZeroReturnsEmptyVector)
{
    std::unique_ptr<IFileDescriptor> localFd =
        std::make_unique<FileDescriptor>(m_localFd, m_descriptorSyscalls);
    UnixSocketConnection connection(std::move(localFd), m_socketSyscalls);
    const std::unique_ptr<IDeadline> deadline = m_deadlines->startingNow(DEFAULT_TEST_TIMEOUT);

    EXPECT_TRUE(connection.readExactly(NO_BYTES_TRANSFERRED, *deadline).empty());
}

TEST_F(UnixSocketConnectionTest, PeerClosingBeforeEnoughBytesThrowsConnectionClosed)
{
    std::unique_ptr<IFileDescriptor> localFd =
        std::make_unique<FileDescriptor>(m_localFd, m_descriptorSyscalls);
    UnixSocketConnection connection(std::move(localFd), m_socketSyscalls);
    const std::unique_ptr<IDeadline> deadline = m_deadlines->startingNow(DEFAULT_TEST_TIMEOUT);

    sendBytes(m_peerFd, SHORT_CHUNK);
    closePeer();

    EXPECT_THROW(connection.readExactly(FOUR_BYTES, *deadline),
                 adapter_socket::ConnectionClosedException);
}

TEST_F(UnixSocketConnectionTest, WriteAllRoundTripsThroughPeer)
{
    std::unique_ptr<IFileDescriptor> localFd =
        std::make_unique<FileDescriptor>(m_localFd, m_descriptorSyscalls);
    UnixSocketConnection connection(std::move(localFd), m_socketSyscalls);
    const std::unique_ptr<IDeadline> deadline = m_deadlines->startingNow(DEFAULT_TEST_TIMEOUT);

    connection.writeAll(WRITTEN_BYTES, *deadline);

    EXPECT_EQ(receiveBytes(m_peerFd, THREE_BYTES), WRITTEN_BYTES);
}

TEST_F(UnixSocketConnectionTest, LargeWriteCompletesWhileReaderDrains)
{
    std::unique_ptr<IFileDescriptor> localFd =
        std::make_unique<FileDescriptor>(m_localFd, m_descriptorSyscalls);
    UnixSocketConnection connection(std::move(localFd), m_socketSyscalls);
    const std::vector<uint8_t> payload(LARGE_PAYLOAD_BYTES, PAYLOAD_FILL_BYTE);
    const std::unique_ptr<IDeadline> deadline = m_deadlines->startingNow(DEFAULT_TEST_TIMEOUT);

    std::thread writer(
        [&connection, &payload, &deadline]() { connection.writeAll(payload, *deadline); });

    const std::vector<uint8_t> received = receiveBytes(m_peerFd, payload.size());
    writer.join();

    EXPECT_TRUE(received == payload);
}

TEST_F(UnixSocketConnectionTest, IdlePeerReadTimesOutWithEtimedout)
{
    std::unique_ptr<IFileDescriptor> localFd =
        std::make_unique<FileDescriptor>(m_localFd, m_descriptorSyscalls);
    UnixSocketConnection connection(std::move(localFd), m_socketSyscalls);

    bool timedOut = false;
    try
    {
        static_cast<void>(connection.readExactly(SINGLE_BYTE, *expiredDeadline()));
    }
    catch (const std::system_error& error)
    {
        timedOut = error.code().value() == ETIMEDOUT;
    }

    EXPECT_TRUE(timedOut);
}

TEST_F(UnixSocketConnectionTest, SlowLorisPeerHitsTotalDeadlineNotPerReadTimeout)
{
    std::unique_ptr<IFileDescriptor> localFd =
        std::make_unique<FileDescriptor>(m_localFd, m_descriptorSyscalls);
    UnixSocketConnection connection(std::move(localFd), m_socketSyscalls);
    std::atomic<bool> stopRequested{false};
    std::thread trickler([this, &stopRequested]() { trickleBytes(m_peerFd, stopRequested); });
    const StopAndJoin joinTrickler(stopRequested, trickler);

    const auto start = std::chrono::steady_clock::now();
    const std::unique_ptr<IDeadline> deadline = m_deadlines->startingNow(SLOW_LORIS_TIMEOUT);
    try
    {
        static_cast<void>(connection.readExactly(SLOW_LORIS_BYTE_COUNT, *deadline));
        ADD_FAILURE() << "readExactly returned before the total deadline";
    }
    catch (const std::system_error& error)
    {
        EXPECT_EQ(error.code().value(), ETIMEDOUT);
    }

    const auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_GE(elapsed, SLOW_LORIS_TIMEOUT);
    EXPECT_LT(elapsed, SLOW_LORIS_TIMEOUT + DEADLINE_UPPER_MARGIN);
}

TEST_F(UnixSocketConnectionTest, WriteAllDeadlineExpiresWhenPeerNeverReads)
{
    std::unique_ptr<IFileDescriptor> localFd =
        std::make_unique<FileDescriptor>(m_localFd, m_descriptorSyscalls);
    UnixSocketConnection connection(std::move(localFd), m_socketSyscalls);
    const std::vector<uint8_t> payload(BLOCKED_WRITE_PAYLOAD_BYTES, PAYLOAD_FILL_BYTE);
    const std::unique_ptr<IDeadline> deadline = m_deadlines->startingNow(BLOCKED_WRITE_TIMEOUT);
    const auto start = std::chrono::steady_clock::now();

    try
    {
        connection.writeAll(payload, *deadline);
        ADD_FAILURE() << "writeAll completed although the peer never read";
    }
    catch (const std::system_error& error)
    {
        EXPECT_EQ(error.code().value(), ETIMEDOUT);
    }

    const auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_GE(elapsed, BLOCKED_WRITE_TIMEOUT);
    EXPECT_LT(elapsed, BLOCKED_WRITE_TIMEOUT + DEADLINE_UPPER_MARGIN);
}

using TransferResult = SyscallResult<std::size_t>;

class UnixSocketConnectionSyscallsTest : public ::testing::Test
{
protected:
    /* LEARN: a connection only transfers bytes; accepting and reading credentials belong to the
       listener. Each test also rules out the other direction: a read never sends, a write never
       receives.
    */
    void SetUp() override
    {
        EXPECT_CALL(*m_syscalls, acceptConnection(_)).Times(0);
        EXPECT_CALL(*m_syscalls, readPeerCredentials(_)).Times(0);
    }

    static TransferResult transferFailure(std::errc error)
    {
        return TransferResult::failure(std::make_error_code(error));
    }

    static auto deliver(const std::vector<uint8_t>& chunk)
    {
        return [chunk](int, std::span<uint8_t> destination) -> TransferResult {
            std::copy(chunk.begin(), chunk.end(), destination.begin());
            return TransferResult::success(chunk.size());
        };
    }

    auto acceptUpTo(std::size_t byteCount)
    {
        return [this, byteCount](int, std::span<const uint8_t> source) -> TransferResult {
            const std::size_t taken = std::min(byteCount, source.size());
            m_sentBytes.insert(m_sentBytes.end(), source.begin(), source.begin() + taken);
            return TransferResult::success(taken);
        };
    }

    std::unique_ptr<UnixSocketConnection> buildConnection()
    {
        std::unique_ptr<MockFileDescriptor> connectedFd = std::make_unique<MockFileDescriptor>();
        EXPECT_CALL(*connectedFd, value()).WillRepeatedly(Return(FAKE_CONNECTED_FD));
        EXPECT_CALL(*connectedFd, waitUntilReady(_, _)).WillRepeatedly(Return());
        return std::make_unique<UnixSocketConnection>(std::move(connectedFd), m_syscalls);
    }

    static int errorValueOf(const std::function<void()>& operation)
    {
        try
        {
            operation();
        }
        catch (const std::system_error& error)
        {
            return error.code().value();
        }
        return NO_ERROR_CAUGHT;
    }

    std::shared_ptr<MockSocketSyscalls> m_syscalls = std::make_shared<MockSocketSyscalls>();
    MockDeadline m_deadline;
    std::vector<uint8_t> m_sentBytes;
};

TEST_F(UnixSocketConnectionSyscallsTest, ReadExactlyWaitsForReadableBeforeEachReceive)
{
    EXPECT_CALL(*m_syscalls, sendSome(_, _)).Times(0);
    std::unique_ptr<MockFileDescriptor> connectedFd = std::make_unique<MockFileDescriptor>();
    EXPECT_CALL(*connectedFd, value()).WillRepeatedly(Return(FAKE_CONNECTED_FD));
    {
        const InSequence inOrder;
        EXPECT_CALL(*connectedFd, waitUntilReady(Readiness::Readable, _));
        EXPECT_CALL(*m_syscalls, receiveSome(FAKE_CONNECTED_FD, _)).WillOnce(deliver(SHORT_CHUNK));
        EXPECT_CALL(*connectedFd, waitUntilReady(Readiness::Readable, _));
        EXPECT_CALL(*m_syscalls, receiveSome(FAKE_CONNECTED_FD, _)).WillOnce(deliver(SHORT_CHUNK));
    }
    UnixSocketConnection connection(std::move(connectedFd), m_syscalls);

    EXPECT_EQ(connection.readExactly(FOUR_BYTES, m_deadline).size(), FOUR_BYTES);
}

TEST_F(UnixSocketConnectionSyscallsTest, ReadExactlyThrowsConnectionClosedWhenReceiveReturnsZero)
{
    EXPECT_CALL(*m_syscalls, sendSome(_, _)).Times(0);
    EXPECT_CALL(*m_syscalls, receiveSome(FAKE_CONNECTED_FD, _))
        .WillOnce(Return(TransferResult::success(NO_BYTES_TRANSFERRED)));
    const std::unique_ptr<UnixSocketConnection> connection = buildConnection();

    EXPECT_THROW(static_cast<void>(connection->readExactly(FIVE_BYTES, m_deadline)),
                 adapter_socket::ConnectionClosedException);
}

const std::vector<ErrnoCase> RETRYABLE_TRANSFER_ERRORS{
    {std::errc::resource_unavailable_try_again, "EAGAIN"},
    {std::errc::operation_would_block, "EWOULDBLOCK"},
    {std::errc::interrupted, "EINTR"},
};

const std::vector<ErrnoCase> FATAL_TRANSFER_ERRORS{
    {std::errc::connection_reset, "ECONNRESET"},     {std::errc::broken_pipe, "EPIPE"},
    {std::errc::bad_file_descriptor, "EBADF"},       {std::errc::not_connected, "ENOTCONN"},
    {std::errc::connection_aborted, "ECONNABORTED"},
};

class UnixSocketConnectionRetryableErrorTest : public UnixSocketConnectionSyscallsTest,
                                               public ::testing::WithParamInterface<ErrnoCase>
{
};

class UnixSocketConnectionFatalErrorTest : public UnixSocketConnectionSyscallsTest,
                                           public ::testing::WithParamInterface<ErrnoCase>
{
};

TEST_P(UnixSocketConnectionRetryableErrorTest, ReadExactlyRetriesAndKeepsAssemblingChunks)
{
    EXPECT_CALL(*m_syscalls, sendSome(_, _)).Times(0);
    {
        const InSequence inOrder;
        EXPECT_CALL(*m_syscalls, receiveSome(FAKE_CONNECTED_FD, _)).WillOnce(deliver(FIRST_CHUNK));
        EXPECT_CALL(*m_syscalls, receiveSome(FAKE_CONNECTED_FD, _))
            .WillOnce(Return(transferFailure(GetParam().error)));
        EXPECT_CALL(*m_syscalls, receiveSome(FAKE_CONNECTED_FD, _)).WillOnce(deliver(SECOND_CHUNK));
    }
    const std::unique_ptr<UnixSocketConnection> connection = buildConnection();

    EXPECT_EQ(connection->readExactly(FIVE_BYTES, m_deadline), BOTH_CHUNKS);
}

TEST_P(UnixSocketConnectionRetryableErrorTest, WriteAllRetriesAndResumesAfterPartialSend)
{
    EXPECT_CALL(*m_syscalls, receiveSome(_, _)).Times(0);
    {
        const InSequence inOrder;
        EXPECT_CALL(*m_syscalls, sendSome(FAKE_CONNECTED_FD, _)).WillOnce(acceptUpTo(TWO_BYTES));
        EXPECT_CALL(*m_syscalls, sendSome(FAKE_CONNECTED_FD, _))
            .WillOnce(Return(transferFailure(GetParam().error)));
        EXPECT_CALL(*m_syscalls, sendSome(FAKE_CONNECTED_FD, _)).WillOnce(acceptUpTo(TWO_BYTES));
        EXPECT_CALL(*m_syscalls, sendSome(FAKE_CONNECTED_FD, _)).WillOnce(acceptUpTo(TWO_BYTES));
    }
    const std::unique_ptr<UnixSocketConnection> connection = buildConnection();

    connection->writeAll(BOTH_CHUNKS, m_deadline);

    EXPECT_EQ(m_sentBytes, BOTH_CHUNKS);
}

TEST_P(UnixSocketConnectionFatalErrorTest, ReadExactlyThrowsTheReceiveErrno)
{
    EXPECT_CALL(*m_syscalls, sendSome(_, _)).Times(0);
    EXPECT_CALL(*m_syscalls, receiveSome(FAKE_CONNECTED_FD, _))
        .WillOnce(Return(transferFailure(GetParam().error)));
    const std::unique_ptr<UnixSocketConnection> connection = buildConnection();

    const int errorValue = errorValueOf([&connection, this]() {
        static_cast<void>(connection->readExactly(FIVE_BYTES, m_deadline));
    });

    EXPECT_EQ(errorValue, static_cast<int>(GetParam().error));
}

TEST_P(UnixSocketConnectionFatalErrorTest, WriteAllThrowsTheSendErrno)
{
    EXPECT_CALL(*m_syscalls, receiveSome(_, _)).Times(0);
    EXPECT_CALL(*m_syscalls, sendSome(FAKE_CONNECTED_FD, _))
        .WillOnce(Return(transferFailure(GetParam().error)));
    const std::unique_ptr<UnixSocketConnection> connection = buildConnection();

    const int errorValue =
        errorValueOf([&connection, this]() { connection->writeAll(WRITTEN_BYTES, m_deadline); });

    EXPECT_EQ(errorValue, static_cast<int>(GetParam().error));
}

INSTANTIATE_TEST_SUITE_P(RetryableErrors, UnixSocketConnectionRetryableErrorTest,
                         ::testing::ValuesIn(RETRYABLE_TRANSFER_ERRORS), caseName<ErrnoCase>);

INSTANTIATE_TEST_SUITE_P(FatalErrors, UnixSocketConnectionFatalErrorTest,
                         ::testing::ValuesIn(FATAL_TRANSFER_ERRORS), caseName<ErrnoCase>);
