#include <adapter_socket/deadline/steady_deadline_factory.h>

#include <adapter_socket/deadline/steady_deadline.h>

#include <memory>

namespace adapter_socket {

std::unique_ptr<IDeadline> SteadyDeadlineFactory::startingNow(
    std::chrono::milliseconds timeout) const
{
    return std::make_unique<SteadyDeadline>(timeout);
}

} // namespace adapter_socket
