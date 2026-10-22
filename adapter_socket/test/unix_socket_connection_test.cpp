#include <adapter_socket/unix_socket_connection.h>

#include <adapter_socket/exceptions/socket_exception.h>

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

constexpr std::size_t LARGE_PAYLOAD_BYTES = 1024 * 1024;
constexpr std::size_t BLOCKED_WRITE_PAYLOAD_BYTES = 4 * 1024 * 1024;
constexpr std::size_t SLOW_LORIS_BYTE_COUNT = 10;
constexpr std::size_t NO_BYTES_TRANSFERRED = 0;
constexpr std::chrono::milliseconds ZERO_TIMEOUT(0);
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
constexpr std::size_t LOCAL_END = 0;
constexpr std::size_t PEER_END = 1;
constexpr uint8_t SLOW_LORIS_BYTE = 0x2A;
constexpr std::chrono::milliseconds SLOW_LORIS_INTERVAL(150);

void sendBytes(int fd, const std::vector<uint8_t>& bytes)
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

std::vector<uint8_t> receiveBytes(int fd, std::size_t byteCount)
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

void trickleBytes(int fd, const std::atomic<bool>& stopRequested)
{
    std::size_t sentCount = NO_BYTES_TRANSFERRED;
    while (sentCount < SLOW_LORIS_BYTE_COUNT && !stopRequested.load())
    {
        const std::vector<uint8_t> oneByte{SLOW_LORIS_BYTE};
        if (::send(fd, oneByte.data(), oneByte.size(), MSG_NOSIGNAL) == SYSCALL_ERROR)
        {
            return;
        }
        std::this_thread::sleep_for(SLOW_LORIS_INTERVAL);
        ++sentCount;
    }
}

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

} // namespace

class UnixSocketConnectionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        std::array<int, 2> fds{};
        if (::socketpair(AF_UNIX, SOCK_STREAM, DEFAULT_PROTOCOL, fds.data()) != SYSCALL_SUCCESS)
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
            m_peerFd = -1;
        }
    }

    int m_localFd = -1;
    int m_peerFd = -1;
};

TEST_F(UnixSocketConnectionTest, ReadExactlyAssemblesBytesWrittenInTwoSends)
{
    adapter_socket::UnixSocketConnection connection(m_localFd);

    sendBytes(m_peerFd, {1, 2, 3});
    sendBytes(m_peerFd, {4, 5});

    EXPECT_EQ(connection.readExactly(5), (std::vector<uint8_t>{1, 2, 3, 4, 5}));
}

TEST_F(UnixSocketConnectionTest, ReadExactlyZeroReturnsEmptyVector)
{
    adapter_socket::UnixSocketConnection connection(m_localFd);

    EXPECT_TRUE(connection.readExactly(NO_BYTES_TRANSFERRED).empty());
}

TEST_F(UnixSocketConnectionTest, ZeroTimeoutIsRejectedAndTheDescriptorIsClosed)
{
    EXPECT_THROW(adapter_socket::UnixSocketConnection(m_localFd, ZERO_TIMEOUT), std::invalid_argument);

    const std::vector<uint8_t> probe{1};
    const ssize_t result = ::send(m_localFd, probe.data(), probe.size(), MSG_NOSIGNAL);
    const int savedErrno = errno;

    EXPECT_EQ(result, SYSCALL_ERROR);
    EXPECT_EQ(savedErrno, EBADF);
}

TEST_F(UnixSocketConnectionTest, PeerClosingBeforeEnoughBytesThrowsConnectionClosed)
{
    adapter_socket::UnixSocketConnection connection(m_localFd);

    sendBytes(m_peerFd, {7, 8});
    closePeer();

    EXPECT_THROW(connection.readExactly(4), adapter_socket::ConnectionClosedException);
}

TEST_F(UnixSocketConnectionTest, WriteAllRoundTripsThroughPeer)
{
    adapter_socket::UnixSocketConnection connection(m_localFd);

    connection.writeAll({10, 20, 30});

    EXPECT_EQ(receiveBytes(m_peerFd, 3), (std::vector<uint8_t>{10, 20, 30}));
}

TEST_F(UnixSocketConnectionTest, LargeWriteCompletesWhileReaderDrains)
{
    adapter_socket::UnixSocketConnection connection(m_localFd);
    const std::vector<uint8_t> payload(LARGE_PAYLOAD_BYTES, 0x5A);

    std::thread writer([&connection, &payload]() {
        connection.writeAll(payload);
    });

    const std::vector<uint8_t> received = receiveBytes(m_peerFd, payload.size());
    writer.join();

    EXPECT_TRUE(received == payload);
}

TEST_F(UnixSocketConnectionTest, PeerCredentialsReportCurrentUserId)
{
    adapter_socket::UnixSocketConnection connection(m_localFd);

    EXPECT_EQ(connection.peerCredentials().userId, getuid());
}

TEST_F(UnixSocketConnectionTest, IdlePeerReadTimesOutWithEtimedout)
{
    adapter_socket::UnixSocketConnection connection(m_localFd, IDLE_READ_TIMEOUT);
    const auto start = std::chrono::steady_clock::now();

    try
    {
        connection.readExactly(SINGLE_BYTE);
        ADD_FAILURE() << "readExactly returned without data";
    }
    catch (const std::system_error& error)
    {
        EXPECT_EQ(error.code().value(), ETIMEDOUT);
    }

    const auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_GE(elapsed, IDLE_READ_TIMEOUT);
    EXPECT_LT(elapsed, IDLE_READ_TIMEOUT + DEADLINE_UPPER_MARGIN);
}

TEST_F(UnixSocketConnectionTest, SlowLorisPeerHitsTotalDeadlineNotPerReadTimeout)
{
    adapter_socket::UnixSocketConnection connection(m_localFd, SLOW_LORIS_TIMEOUT);
    std::atomic<bool> stopRequested{false};
    std::thread trickler([this, &stopRequested]() {
        trickleBytes(m_peerFd, stopRequested);
    });
    const StopAndJoin joinTrickler(stopRequested, trickler);

    const auto start = std::chrono::steady_clock::now();
    try
    {
        connection.readExactly(SLOW_LORIS_BYTE_COUNT);
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
    adapter_socket::UnixSocketConnection connection(m_localFd, BLOCKED_WRITE_TIMEOUT);
    const std::vector<uint8_t> payload(BLOCKED_WRITE_PAYLOAD_BYTES, 0x5A);
    const auto start = std::chrono::steady_clock::now();

    try
    {
        connection.writeAll(payload);
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
