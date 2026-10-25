#ifndef ADAPTER_SOCKET_EXCEPTIONS_SOCKET_EXCEPTION_H
#define ADAPTER_SOCKET_EXCEPTIONS_SOCKET_EXCEPTION_H

#include <stdexcept>
#include <string>

namespace adapter_socket {

class ConnectionClosedException final : public std::runtime_error
{
public:
    explicit ConnectionClosedException(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

class FrameTooLargeException final : public std::runtime_error
{
public:
    explicit FrameTooLargeException(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

class ActivationException final : public std::runtime_error
{
public:
    explicit ActivationException(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

} // namespace adapter_socket

#endif /* ADAPTER_SOCKET_EXCEPTIONS_SOCKET_EXCEPTION_H */
