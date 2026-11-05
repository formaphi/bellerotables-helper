#ifndef ADAPTER_SOCKET_SYSCALL_RESULT_H
#define ADAPTER_SOCKET_SYSCALL_RESULT_H

#include <system_error>
#include <utility>

namespace adapter_socket {

/* LEARN: what an adapter returns instead of the C convention "-1 and errno". It carries either a
   value or the std::error_code of the failure, and the caller decides whether to retry or throw.
   std::expected would fit, but libstdc++ 13 hides it from clang 18 (clang-tidy in the sandbox).
*/
template <typename Value>
struct SyscallResult
{
    Value value{};
    std::error_code error;

    [[nodiscard]] static SyscallResult success(Value resultValue)
    {
        SyscallResult result;
        result.value = std::move(resultValue);
        return result;
    }

    [[nodiscard]] static SyscallResult failure(std::error_code resultError)
    {
        SyscallResult result;
        result.error = resultError;
        return result;
    }

    [[nodiscard]] bool hasFailed() const
    {
        return static_cast<bool>(error);
    }
};

} // namespace adapter_socket

#endif /* ADAPTER_SOCKET_SYSCALL_RESULT_H */
