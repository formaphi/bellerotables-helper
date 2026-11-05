#ifndef ADAPTER_SOCKET_STEADY_DEADLINE_H
#define ADAPTER_SOCKET_STEADY_DEADLINE_H

#include <adapter_socket/deadline/interface_deadline.h>
#include <chrono>

namespace adapter_socket {

class SteadyDeadline final : public IDeadline
{
public:
    explicit SteadyDeadline(std::chrono::milliseconds timeout);
    ~SteadyDeadline() override = default;

    SteadyDeadline(const SteadyDeadline&) = delete;
    SteadyDeadline& operator=(const SteadyDeadline&) = delete;
    SteadyDeadline(SteadyDeadline&&) = delete;
    SteadyDeadline& operator=(SteadyDeadline&&) = delete;

    [[nodiscard]] bool hasExpired() const override;
    [[nodiscard]] int pollTimeoutMilliseconds() const override;

private:
    [[nodiscard]] std::chrono::milliseconds::rep remainingMilliseconds() const;

    std::chrono::steady_clock::time_point m_expiresAt;
};

} // namespace adapter_socket

#endif /* ADAPTER_SOCKET_STEADY_DEADLINE_H */
