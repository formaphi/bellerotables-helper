#ifndef ADAPTER_SOCKET_FILE_DESCRIPTOR_H
#define ADAPTER_SOCKET_FILE_DESCRIPTOR_H

#include <adapter_socket/deadline/interface_deadline.h>
#include <adapter_socket/file_descriptor/interface_file_descriptor.h>

namespace adapter_socket {

class FileDescriptor final : public IFileDescriptor
{
public:
    explicit FileDescriptor(int fileDescriptor);
    ~FileDescriptor() override;

    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;
    FileDescriptor(FileDescriptor&&) = delete;
    FileDescriptor& operator=(FileDescriptor&&) = delete;

    [[nodiscard]] int value() const override;
    void waitUntilReady(Readiness readiness, const IDeadline& deadline) const override;

private:
    static constexpr int INVALID_FD = -1;
    static constexpr int VALID_FD = 0;

    [[nodiscard]] static short pollEventsFor(Readiness readiness);

    int m_fd;
};

} // namespace adapter_socket

#endif /* ADAPTER_SOCKET_FILE_DESCRIPTOR_H */
