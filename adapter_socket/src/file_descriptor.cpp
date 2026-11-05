#include "adapter_socket/file_descriptor/file_descriptor.h"

#include <system_error>
#include <utility>

namespace adapter_socket {

constexpr const char* DEADLINE_EXCEEDED_MESSAGE = "poll deadline exceeded";

FileDescriptor::FileDescriptor(int fileDescriptor, std::shared_ptr<IDescriptorSyscalls> syscalls)
    : m_fd(fileDescriptor)
    , m_syscalls(std::move(syscalls))
{
}

FileDescriptor::~FileDescriptor()
{
    const bool isOpen = m_fd >= VALID_FD;
    if (isOpen)
    {
        m_syscalls->closeDescriptor(m_fd);
        m_fd = INVALID_FD;
    }
}

int FileDescriptor::value() const
{
    return m_fd;
}

void FileDescriptor::waitUntilReady(Readiness readiness, const IDeadline& deadline) const
{
    while (true)
    {
        const bool hasExpired = deadline.hasExpired();
        if (hasExpired)
        {
            throw std::system_error(std::make_error_code(std::errc::timed_out),
                                    DEADLINE_EXCEEDED_MESSAGE);
        }

        const int timeoutMilliseconds = deadline.pollTimeoutMilliseconds();
        const SyscallResult<PollOutcome> outcome =
            m_syscalls->pollOnce(m_fd, readiness, timeoutMilliseconds);
        if (!outcome.hasFailed())
        {
            if (outcome.value == PollOutcome::Ready)
            {
                return;
            }
            // LEARN: poll timed out on a millisecond boundary; the loop head decides whether the
            // deadline really passed.
            continue;
        }

        if (outcome.error == std::errc::interrupted)
        {
            // LEARN: EINTR means a signal interrupted poll before it reported readiness, so
            // polling again is safe and not an error.
            continue;
        }
        throw std::system_error(outcome.error, "poll");
    }
}

} // namespace adapter_socket
