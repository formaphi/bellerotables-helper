# Task description
Refactor the adapter_socket/ moving every raw POSIX call behind a small adapter interface, so the classes that hold this logic must stop calling the C library directly instead will call the adapters and then  every OS failure can be simulated in a unit test.

# How to judge completion

Done when AgentUserPolicy, UnixSocketConnection, UnixSocketListener, SystemdActivation, use interfaces instead C libraries to make a syscall, and no regressions occur.

# Other notes for the reviewer

## The project in short

bellerotables-helper is a one-shot C++23 binary, socket-activated by systemd, that reads one length-prefixed frame from the agent over a Unix stream socket, applies iptables rules and reports the real kernel state. It runs with CAP_NET_ADMIN only. adapter_socket/ is the socket layer, already implemented and merged on develop: SocketEndpoint::acceptAuthorized() takes the activated listening fd, accepts exactly one connection, checks the peer uid against the agent user from /etc/bellerophon/bellerotables-helper.conf (SO_PEERCRED), and returns a framed connection. Every wait goes through IFileDescriptor::waitUntilReady with a per-operation IDeadline.

Reference example used throughout: config agent_user=root resolves to uid -1, so a peer with uid 0 is allowed and a peer with uid 1000 is refused. For the socket side, a request of 10 bytes arriving as 4 + 6 with an EINTR in between must still come out as the same 10 bytes in order.

## Known gaps left on purpose

- sd_listen_fds / sd_is_socket_unix stay direct (see Out of scope).
- poll and close stay inside FileDescriptor; their failure paths keep being covered through real descriptors.
- The adapters are built and tested but not yet wired into a running binary, because main.cpp is empty until the parser and controller exist.
- The tests that already use real sockets stay as integration-style tests on purpose; the new mock-based tests add the failure cases, they do not replace them.

## Out of scope

- sd_listen_fds and sd_is_socket_unix in SystemdActivation. They are libsystemd calls, not POSIX, and the existing tests already drive them with a real socket and the LISTEN_* environment.
- Replacing the while loops with futures, coroutines or epoll. The loops are the poll option, and the others do not pay off in a one-shot process with one connection.
- Changes to deadlines, framing, the wire protocol, the .conf format or the peer-credential flow.
- Wiring the adapters into main.cpp, the parser, the controller and adapter_iptc. main.cpp is still an empty main.

## General Idea how they interact

Each class that used to call the C library now calls an interface. A POSIX class behind the interface makes the real call, and tests put a mock in its place. ErrnoClassifier is a set of pure functions, so it has no interface.

```
AgentUserPolicy ------> IFileReader ----------> PosixFileReader ------> ifstream
        |
        +-------------> IUserDirectory -------> PosixUserDirectory ---> sysconf, getpwnam_r

UnixSocketListener ---> ISocketSyscalls -----> PosixSocketSyscalls --> accept4, getsockopt
UnixSocketConnection -> ISocketSyscalls ------^                       recv, send
        |
        +-- asks ErrnoClassifier whether a failed call is worth retrying

SystemdActivation ----> IFileDescriptor ------> FileDescriptor -------> fcntl, poll, close
```


## Evidence the task is complete, and pointers for the reviewer

- The files are really changed.
- The make test-unit passes
- The make test-asan, make test-valgrind, make format-check and make tidy-check passes.

## How you verified it, which files to look at

1. make rebuild-unit && make test-unit
Expected: all PASS.
3. No raw calls outside the adapters.
grep -rnE "::(recv|send|accept4|getsockopt|getpwnam_r|sysconf|fcntl)\b" adapter_socket/src
matches only the POSIX implementation files (and FileDescriptor for fcntl).
grep -n "ifstream" adapter_socket/src
matches only the file reader. <pwd.h>, <unistd.h> and <sys/socket.h> are no longer included by agent_user_policy.cpp, unix_socket_connection.cpp and unix_socket_listener.cpp for calls.
10. make rebuild-asan && make test-asan
and
make rebuild-valgrind && make test-valgrind
pass with no sanitizer errors and no leaks.
make format-check
and
make tidy-check
report nothing.
Real-socket behavior is preserved: the existing UnixSocketConnection, UnixSocketListener, FramedConnection and SocketEndpoint tests that use socketpair() or real sockets still pass, unchanged in what they assert.

## Files to read, in this order:

1. docs/syscall-adapters.md (the reasoning and the options that were rejected)
2. adapter_socket/src/agent_user_policy.cpp, unix_socket_connection.cpp, unix_socket_listener.cpp, systemd_activation.cpp and file_descriptor.cpp, which now hold logic only and no raw calls
3. The new interfaces and their POSIX classes under adapter_socket/include/adapter_socket/user_directory/, file_reader/ and socket_syscalls/, then syscall_errors/errno_classifier.h
4. The mocks in adapter_socket/include/adapter_socket/mock/ (mock_user_directory.h, mock_file_reader.h, mock_socket_syscalls.h)
5. The tests: *_syscalls_test.cpp, posix_*_test.cpp, errno_classifier_test.cpp and agent_user_policy_test.cpp
6. adapter_socket/include/adapter_socket/composition/system_adapters.h and system_adapters_test.cpp
