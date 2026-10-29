#if !defined(INTERFACE_DEADLINE_FACTORY_H)
#define INTERFACE_DEADLINE_FACTORY_H

#include <adapter_socket/deadline/interface_deadline.h>

#include <chrono>
#include <memory>

namespace adapter_socket {

class IDeadlineFactory
{
public:
    virtual ~IDeadlineFactory() = default;

    [[nodiscard]] virtual std::unique_ptr<IDeadline> startingNow(
        std::chrono::milliseconds timeout) const = 0;
};

} // namespace adapter_socket

#endif // INTERFACE_DEADLINE_FACTORY_H
