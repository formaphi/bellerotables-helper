#include <adapter_socket/file_descriptor/file_descriptor.h>

#include <poll.h>
#include <unistd.h>
#include <cerrno>
#include <system_error>

namespace adapter_socket {

constexpr nfds_t SINGLE_POLL_DESCRIPTOR = 1;
constexpr int POLL_TIMED_OUT = 0;
constexpr const char* DEADLINE_EXCEEDED_MESSAGE = "poll deadline exceeded";

FileDescriptor::FileDescriptor(int fileDescriptor)
    : m_fd(fileDescriptor)
{
}

FileDescriptor::~FileDescriptor()
{
    const bool isOpen = m_fd >= VALID_FD;
    if (isOpen)
    {
        ::close(m_fd);
        m_fd = INVALID_FD;
    }
}

int FileDescriptor::value() const
{
    return m_fd;
}

void FileDescriptor::waitUntilReady(Readiness readiness, const IDeadline& deadline) const
{
    pollfd descriptor{};
    descriptor.fd = m_fd;
    descriptor.events = pollEventsFor(readiness);

    while (true)
    {
        const bool hasExpired = deadline.hasExpired();
        if (hasExpired)
        {
            throw std::system_error(ETIMEDOUT, std::generic_category(), DEADLINE_EXCEEDED_MESSAGE);
        }

        const int timeoutMilliseconds = deadline.pollTimeoutMilliseconds();

        const int pollResult = ::poll(&descriptor, SINGLE_POLL_DESCRIPTOR, timeoutMilliseconds);
        /*LEARN
            suspends execution of the calling thread until the file descriptor is ready 
            for the requested I/O operation or until the timeout expires.
            The poll function is used to monitor multiple file descriptors to see if
            any of them are ready for I/O. In this case, 
            it is used to wait for the file descriptor to become ready for reading or writing, 
            depending on the specified readiness.
        */
       
        if (pollResult > POLL_TIMED_OUT)
        {
            return;
        }

        if (pollResult == POLL_TIMED_OUT)
        {
            // poll timed out on a millisecond boundary; the loop head decides whether the deadline
            // really passed.
            continue;
        }

        const int savedErrno = errno;
        if (savedErrno == EINTR)
        {
            // EINTR: a signal interrupted poll before it reported readiness, so polling again is
            // safe and not an error.
            continue;
        }
        throw std::system_error(savedErrno, std::generic_category(), "poll");
    }
}

short FileDescriptor::pollEventsFor(Readiness readiness)
{
    const bool isWritable = readiness == Readiness::Writable;
    return isWritable ? POLLOUT : POLLIN;
}

} // namespace adapter_socket
