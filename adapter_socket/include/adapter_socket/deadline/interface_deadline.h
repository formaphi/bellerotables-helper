#if !defined(INTERFACE_DEADLINE_H)
#define INTERFACE_DEADLINE_H

namespace adapter_socket {

class IDeadline
{
public:
    virtual ~IDeadline() = default;

    [[nodiscard]] virtual bool hasExpired() const = 0;
    [[nodiscard]] virtual int pollTimeoutMilliseconds() const = 0;
};

} // namespace adapter_socket

#endif // INTERFACE_DEADLINE_H
