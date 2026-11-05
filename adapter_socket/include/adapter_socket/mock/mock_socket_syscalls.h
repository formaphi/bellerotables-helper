#pragma once

#include "adapter_socket/socket_syscalls/interface_socket_syscalls.h"

#include <gmock/gmock.h>

namespace adapter_socket {

class MockSocketSyscalls : public ISocketSyscalls
{
public:
    MOCK_METHOD(SyscallResult<int>, acceptConnection, (int listeningFd), (const, override));
    MOCK_METHOD(SyscallResult<std::size_t>, receiveSome,
                (int connectedFd, std::span<uint8_t> destination), (const, override));
    MOCK_METHOD(SyscallResult<std::size_t>, sendSome,
                (int connectedFd, std::span<const uint8_t> source), (const, override));
    MOCK_METHOD(SyscallResult<PeerCredentials>, readPeerCredentials, (int connectedFd),
                (const, override));
};

} // namespace adapter_socket
