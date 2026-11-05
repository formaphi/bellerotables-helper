#ifndef ADAPTER_SOCKET_STEADY_DEADLINE_FACTORY_H
#define ADAPTER_SOCKET_STEADY_DEADLINE_FACTORY_H

#include "adapter_socket/deadline/interface_deadline_factory.h"

namespace adapter_socket {

class SteadyDeadlineFactory final : public IDeadlineFactory
{
public:
    SteadyDeadlineFactory() = default;
    ~SteadyDeadlineFactory() override = default;

    SteadyDeadlineFactory(const SteadyDeadlineFactory&) = delete;
    SteadyDeadlineFactory& operator=(const SteadyDeadlineFactory&) = delete;
    SteadyDeadlineFactory(SteadyDeadlineFactory&&) = delete;
    SteadyDeadlineFactory& operator=(SteadyDeadlineFactory&&) = delete;

    [[nodiscard]] std::unique_ptr<IDeadline> startingNow(
        std::chrono::milliseconds timeout) const override;
};

} // namespace adapter_socket

#endif /* ADAPTER_SOCKET_STEADY_DEADLINE_FACTORY_H */
