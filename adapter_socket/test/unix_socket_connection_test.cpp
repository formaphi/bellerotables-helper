#include <adapter_socket/socket_connection/unix_socket_connection.h>

#include <adapter_socket/deadline/interface_deadline.h>
#include <adapter_socket/deadline/interface_deadline_factory.h>
#include <adapter_socket/deadline/steady_deadline_factory.h>
#include <adapter_socket/exceptions/socket_exception.h>
#include <adapter_socket/file_descriptor/file_descriptor.h>
#include <adapter_socket/file_descriptor/interface_file_descriptor.h>
#include <adapter_socket/mock/mock_deadline.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

using adapter_socket::FileDescriptor;
using adapter_socket::IDeadline;
using adapter_socket::IDeadlineFactory;
using adapter_socket::IFileDescriptor;
using adapter_socket::MockDeadline;
using adapter_socket::SteadyDeadlineFactory;
using adapter_socket::UnixSocketConnection;
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
};

TEST_F(UnixSocketConnectionTest, ReadExactlyAssemblesBytesWrittenInTwoSends)
{
    std::unique_ptr<IFileDescriptor> localFd = std::make_unique<FileDescriptor>(m_localFd);
    UnixSocketConnection connection(std::move(localFd));
    const std::unique_ptr<IDeadline> deadline = m_deadlines->startingNow(DEFAULT_TEST_TIMEOUT);

    sendBytes(m_peerFd, FIRST_CHUNK);
    sendBytes(m_peerFd, SECOND_CHUNK);

    EXPECT_EQ(connection.readExactly(FIVE_BYTES, *deadline), BOTH_CHUNKS);
}

TEST_F(UnixSocketConnectionTest, ReadExactlyZeroReturnsEmptyVector)
{
    std::unique_ptr<IFileDescriptor> localFd = std::make_unique<FileDescriptor>(m_localFd);
    UnixSocketConnection connection(std::move(localFd));
    const std::unique_ptr<IDeadline> deadline = m_deadlines->startingNow(DEFAULT_TEST_TIMEOUT);

    EXPECT_TRUE(connection.readExactly(NO_BYTES_TRANSFERRED, *deadline).empty());
}

TEST_F(UnixSocketConnectionTest, PeerClosingBeforeEnoughBytesThrowsConnectionClosed)
{
    std::unique_ptr<IFileDescriptor> localFd = std::make_unique<FileDescriptor>(m_localFd);
    UnixSocketConnection connection(std::move(localFd));
    const std::unique_ptr<IDeadline> deadline = m_deadlines->startingNow(DEFAULT_TEST_TIMEOUT);

    sendBytes(m_peerFd, SHORT_CHUNK);
    closePeer();

    EXPECT_THROW(connection.readExactly(FOUR_BYTES, *deadline),
                 adapter_socket::ConnectionClosedException);
}

TEST_F(UnixSocketConnectionTest, WriteAllRoundTripsThroughPeer)
{
    std::unique_ptr<IFileDescriptor> localFd = std::make_unique<FileDescriptor>(m_localFd);
    UnixSocketConnection connection(std::move(localFd));
    const std::unique_ptr<IDeadline> deadline = m_deadlines->startingNow(DEFAULT_TEST_TIMEOUT);

    connection.writeAll(WRITTEN_BYTES, *deadline);

    EXPECT_EQ(receiveBytes(m_peerFd, THREE_BYTES), WRITTEN_BYTES);
}

TEST_F(UnixSocketConnectionTest, LargeWriteCompletesWhileReaderDrains)
{
    std::unique_ptr<IFileDescriptor> localFd = std::make_unique<FileDescriptor>(m_localFd);
    UnixSocketConnection connection(std::move(localFd));
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
    std::unique_ptr<IFileDescriptor> localFd = std::make_unique<FileDescriptor>(m_localFd);
    UnixSocketConnection connection(std::move(localFd));

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
    std::unique_ptr<IFileDescriptor> localFd = std::make_unique<FileDescriptor>(m_localFd);
    UnixSocketConnection connection(std::move(localFd));
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
    std::unique_ptr<IFileDescriptor> localFd = std::make_unique<FileDescriptor>(m_localFd);
    UnixSocketConnection connection(std::move(localFd));
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
