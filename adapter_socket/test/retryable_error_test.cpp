#include "adapter_socket/socket_syscalls/retryable_error.h"

#include "parameterized_case.h"

#include <gtest/gtest.h>
#include <system_error>
#include <vector>

using adapter_socket::isRetryableAcceptError;
using adapter_socket::isRetryableTransferError;

constexpr bool RETRIED = true;
constexpr bool FATAL = false;

struct RetryableErrorCase
{
    std::errc error;
    bool isRetryableTransfer;
    bool isRetryableAccept;
    const char* name;
};

const std::vector<RetryableErrorCase> ERROR_CLASSIFICATION{
    {std::errc::resource_unavailable_try_again, RETRIED, RETRIED, "EAGAIN"},
    {std::errc::operation_would_block, RETRIED, RETRIED, "EWOULDBLOCK"},
    {std::errc::interrupted, RETRIED, RETRIED, "EINTR"},
    {std::errc::connection_aborted, FATAL, RETRIED, "ECONNABORTED"},
    {std::errc::connection_reset, FATAL, FATAL, "ECONNRESET"},
    {std::errc::broken_pipe, FATAL, FATAL, "EPIPE"},
    {std::errc::bad_file_descriptor, FATAL, FATAL, "EBADF"},
    {std::errc::too_many_files_open, FATAL, FATAL, "EMFILE"},
};

class RetryableErrorTest : public ::testing::TestWithParam<RetryableErrorCase>
{
};

TEST_P(RetryableErrorTest, TransferClassification)
{
    const std::error_code error = std::make_error_code(GetParam().error);

    EXPECT_EQ(isRetryableTransferError(error), GetParam().isRetryableTransfer);
}

TEST_P(RetryableErrorTest, AcceptClassification)
{
    const std::error_code error = std::make_error_code(GetParam().error);

    EXPECT_EQ(isRetryableAcceptError(error), GetParam().isRetryableAccept);
}

INSTANTIATE_TEST_SUITE_P(Errno, RetryableErrorTest, ::testing::ValuesIn(ERROR_CLASSIFICATION),
                         caseName<RetryableErrorCase>);
