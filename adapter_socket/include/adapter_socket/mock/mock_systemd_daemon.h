#pragma once

#include "adapter_socket/systemd_daemon/interface_systemd_daemon.h"

#include <gmock/gmock.h>

namespace adapter_socket {

class MockSystemdDaemon : public ISystemdDaemon
{
public:
    MOCK_METHOD(SyscallResult<std::vector<int>>, takeListenFds, (), (const, override));
    MOCK_METHOD(SyscallResult<bool>, isListeningUnixStream, (int fileDescriptor),
                (const, override));
};

} // namespace adapter_socket
