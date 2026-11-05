#include "adapter_socket/descriptor_syscalls/posix_descriptor_syscalls.h"

#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <system_error>
#include <unistd.h>

namespace adapter_socket {

constexpr nfds_t SINGLE_POLL_DESCRIPTOR = 1;
constexpr int POLL_TIMED_OUT = 0;
constexpr int SYSCALL_ERROR = -1;

namespace {

std::error_code lastSystemError()
{
    const int savedErrno = errno;
    return {savedErrno, std::generic_category()};
}

} // namespace

void PosixDescriptorSyscalls::closeDescriptor(int fileDescriptor) const noexcept
{
    ::close(fileDescriptor);
}

SyscallResult<PollOutcome> PosixDescriptorSyscalls::pollOnce(int fileDescriptor,
                                                             Readiness readiness,
                                                             int timeoutMilliseconds) const
{
    pollfd descriptor{};
    descriptor.fd = fileDescriptor;
    descriptor.events = pollEventsFor(readiness);

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
        return SyscallResult<PollOutcome>::success(PollOutcome::Ready);
    }

    if (pollResult == POLL_TIMED_OUT)
    {
        return SyscallResult<PollOutcome>::success(PollOutcome::TimedOut);
    }

    return SyscallResult<PollOutcome>::failure(lastSystemError());
}

std::error_code PosixDescriptorSyscalls::enableNonBlocking(int fileDescriptor) const
{
    /*LEARN
        fcntl is a system call used to manipulate file descriptors.
        In this context, it is used to read and modify the file status flags
        of the given file descriptor to enable non-blocking mode.
    */
    const int flags = ::fcntl(fileDescriptor, F_GETFL);
    if (flags == SYSCALL_ERROR)
    {
        return lastSystemError();
    }

    const int setResult = ::fcntl(fileDescriptor, F_SETFL, flags | O_NONBLOCK);
    if (setResult == SYSCALL_ERROR)
    {
        return lastSystemError();
    }

    return {};
}

short PosixDescriptorSyscalls::pollEventsFor(Readiness readiness)
{
    const bool isWritable = readiness == Readiness::Writable;
    return isWritable ? POLLOUT : POLLIN;
}

} // namespace adapter_socket
