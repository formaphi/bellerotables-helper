#ifndef ADAPTER_SOCKET_RETRYABLE_ERROR_H
#define ADAPTER_SOCKET_RETRYABLE_ERROR_H

#include <system_error>

namespace adapter_socket {

/* LEARN: these errors mean the call moved no data and took no connection, so waiting for
   readiness again before the same deadline is safe.
   EAGAIN / EWOULDBLOCK: "operation would block" is not a failure; the socket buffer is empty
   (recv) or full (send), or no connection is queued (accept). They are the same value on Linux
   but may differ elsewhere, so both are tested.
   EINTR: a signal interrupted the call before anything moved.
   ECONNABORTED (accept only): the queued connection was reset before it was taken; the next
   one in the backlog may still be fine.
*/
[[nodiscard]] inline bool isRetryableTransferError(const std::error_code& error)
{
    return error == std::errc::resource_unavailable_try_again ||
           error == std::errc::operation_would_block || error == std::errc::interrupted;
}

[[nodiscard]] inline bool isRetryableAcceptError(const std::error_code& error)
{
    return isRetryableTransferError(error) || error == std::errc::connection_aborted;
}

} // namespace adapter_socket

#endif /* ADAPTER_SOCKET_RETRYABLE_ERROR_H */
