#include <adapter_socket/deadline/steady_deadline.h>
#include <algorithm>
#include <limits>

namespace adapter_socket {

constexpr std::chrono::milliseconds::rep NO_TIME_REMAINING = 0;
constexpr std::chrono::milliseconds::rep LARGEST_POLL_MILLISECONDS =
    std::numeric_limits<int>::max();

SteadyDeadline::SteadyDeadline(std::chrono::milliseconds timeout)
    : m_expiresAt(std::chrono::steady_clock::now() + timeout)
{
}

bool SteadyDeadline::hasExpired() const
{
    const std::chrono::milliseconds::rep remaining = remainingMilliseconds();
    return remaining <= NO_TIME_REMAINING;
}

int SteadyDeadline::pollTimeoutMilliseconds() const
{
    const std::chrono::milliseconds::rep remaining = remainingMilliseconds();
    const std::chrono::milliseconds::rep bounded =
        std::clamp(remaining, NO_TIME_REMAINING, LARGEST_POLL_MILLISECONDS);
    return static_cast<int>(bounded);
}

std::chrono::milliseconds::rep SteadyDeadline::remainingMilliseconds() const
{
    const std::chrono::steady_clock::duration untilExpiry =
        m_expiresAt - std::chrono::steady_clock::now();
    // LEARN: ceil, not floor: a truncated poll timeout wakes before the deadline and the loop
    // spins.
    return std::chrono::ceil<std::chrono::milliseconds>(untilExpiry).count();
}

} // namespace adapter_socket
