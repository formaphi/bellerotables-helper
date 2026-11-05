# Syscall adapters in `adapter_socket`

**Status:** proposed 2026-10-28, implemented for every C call in the library.
**Origin:** discussion of where the raw POSIX calls (`getpwnam_r`, `getsockopt`, `recv`, `send`, `accept4`)
should live.

## Problem

POSIX calls were scattered across the classes that hold domain logic:

| File | Calls |
|------|-------|
| `file_descriptor.cpp` | `close`, `poll` |
| `systemd_activation.cpp` | `sd_listen_fds`, `sd_is_socket_unix`, `fcntl` |
| `unix_socket_listener.cpp` | `accept4`, `getsockopt` |
| `unix_socket_connection.cpp` | `recv`, `send` |
| `agent_user_policy.cpp` | `sysconf`, `getpwnam_r` and the `ERANGE` retry loop |
| `peer_credentials.h` | `<sys/socket.h>` in a public header, only to name `ucred` in `fromUcred(const ucred&)` |

`AgentUserPolicy` was the worst case: it read a file, interpreted the config and talked to the OS user
database. The `ERANGE` loop and the "user does not exist" path could only be tested with a real user on the
machine. Retry paths such as `EINTR` in `poll`/`recv` or `ECONNABORTED` in `accept4` could not be provoked with a
real `socketpair()` at all.

## Decision

- **One small interface per kind of C API, not one interface for all of them.** Each interface is an Adapter: it
  turns a C API (`errno`, raw pointers, caller-owned buffers, C structs, macros) into an operation the rest of the
  code can call and mock.

  ```
  IDescriptorSyscalls ← PosixDescriptorSyscalls   close, poll, fcntl(O_NONBLOCK)
  ISocketSyscalls     ← PosixSocketSyscalls       accept4, recv, send, getsockopt(SO_PEERCRED)
  ISystemdDaemon      ← LibsystemdDaemon          sd_listen_fds, sd_is_socket_unix
  IUserDirectory      ← PosixUserDirectory        sysconf, getpwnam_r
  ```

  These four `.cpp` files are the only ones in `src/` that call a C function. The only other C include is
  `<sys/socket.h>` in `peer_credentials.cpp`, for the `ucred` struct definition (see below).

- **The adapter absorbs the C details; the domain class keeps the decisions.**
  - `IUserDirectory::findUserId(name)` hides `sysconf`, `getpwnam_r`, the buffer and the `ERANGE` retry, and returns
    `std::optional<uint32_t>`. `AgentUserPolicy` only reads the config and compares the uid.
  - `ISocketSyscalls` takes `std::span` instead of pointer + length, always passes `MSG_DONTWAIT` (and
    `MSG_NOSIGNAL` on send), and returns `PeerCredentials` instead of `ucred`.
  - `IDescriptorSyscalls::pollOnce` takes a `Readiness` instead of `POLLIN`/`POLLOUT` and returns a `PollOutcome`
    (`Ready` / `TimedOut`).
  - `ISystemdDaemon::takeListenFds` returns the fds themselves, so `SD_LISTEN_FDS_START` and the negative-errno
    convention stay in the adapter.
  - The retry loops that depend on the deadline stay in `FileDescriptor`, `UnixSocketConnection` and
    `UnixSocketListener`, where they are now unit-tested with mocks.
- **Adapters never throw.** They return `SyscallResult<T>` (a value or a `std::error_code`), or a plain
  `std::error_code` when there is no value. The domain class decides whether to retry or throw, and keeps the
  existing exception messages (`"recv"`, `"accept4"`, `"getsockopt SO_PEERCRED"`, ...).
  `std::expected` would fit, but libstdc++ 13 only exposes it when `__cpp_concepts >= 202002L`, which clang 18
  (the sandbox `clang-tidy`) does not define, so `make tidy-check` could not parse it.
- **Pure helpers need no interface.** `isRetryableTransferError` (`EAGAIN`, `EWOULDBLOCK`, `EINTR`) and
  `isRetryableAcceptError` (the same plus `ECONNABORTED`) in `socket_syscalls/retryable_error.h` replace the
  repeated `errno` expressions.
- **Conversions stay on their type.** `PeerCredentials::fromUcred(const ucred&)` is still a static method of
  `PeerCredentials`, called by `PosixSocketSyscalls::readPeerCredentials`. The header only forward-declares
  `struct ucred;` (glibc declares it at global scope), so `<sys/socket.h>` is included by `peer_credentials.cpp`
  alone and not by every file that uses `PeerCredentials`. Its tests pin distinct uid/gid/pid values (a swap
  cannot be seen with a real socket when uid == gid), the largest ids (no narrowing), and the kernel's "no peer"
  answer (`uid`/`gid` = `-1`, which must never become `0`, the uid of root).
- **Same conventions as the rest of the lib:** an `interface_*.h` header, a concrete class, a `mock/mock_*.h`, one
  folder per concept under `include/adapter_socket/`.
- **Ownership is unchanged.** Adapters take and return raw `int` fds and own nothing. `FileDescriptor` still owns
  every fd and closes it through `IDescriptorSyscalls`, so a test that returns a fake fd from a mock never closes
  a real one.
- **Wiring is constructor injection; `std::unique_ptr` unless ownership is really shared.** A class that is the
  only user of an adapter owns it with `std::unique_ptr`. `std::shared_ptr` is kept only where the adapter is
  handed on to objects that outlive the class that received it. Classes only receive the interfaces they use:

  | Class | Receives | Why |
  |-------|----------|-----|
  | `AgentUserPolicy` | `unique_ptr<IUserDirectory>` | used only in the constructor, then released |
  | `SystemdActivation` | `unique_ptr<ISystemdDaemon>` | used only by the activation itself |
  | `SystemdActivation` | `shared_ptr<IDescriptorSyscalls>` | handed to the returned `FileDescriptor`, which outlives it |
  | `FileDescriptor` | `shared_ptr<IDescriptorSyscalls>` | one adapter for the listening fd and every accepted fd |
  | `UnixSocketListener`, `UnixSocketListenerFactory` | `shared_ptr<ISocketSyscalls>`, `shared_ptr<IDescriptorSyscalls>` | handed to every listener / fd / connection they create |
  | `UnixSocketConnection` | `shared_ptr<ISocketSyscalls>` | shared with the listener; the connection is returned to the caller and can outlive the endpoint |

  Turning the shared ones into `unique_ptr` would need one adapter instance per fd (a new factory interface) or
  non-owning references that dangle when a connection outlives the endpoint. Copying a `shared_ptr` never copies
  the adapter, only bumps a reference count, and the parameters are taken by value and moved.

## Testing

- Domain classes are tested with `MockDescriptorSyscalls`, `MockSocketSyscalls`, `MockSystemdDaemon` and
  `MockUserDirectory`, including the paths that a real socket cannot produce (`EINTR`, `ECONNABORTED`,
  `SO_PEERCRED` failure closing the accepted fd, `sd_*` failures, fd 3 not closed before it is validated).
- Each concrete adapter has a contract test against the real OS (`posix_*_test.cpp`, `libsystemd_daemon_test.cpp`).
- The existing `socketpair()` / systemd-environment tests still run end to end with the real adapters injected.

## Rejected

- **`ISystem` with every syscall.** It breaks interface segregation (`AgentUserPolicy` would depend on `recv` and
  `accept4`), needs a large mock per test and grows with every new call.
- **A `posix_utils` of free functions for OS calls.** The dependency is hidden and cannot be replaced by a mock,
  against the injection used everywhere else. Free functions are fine only for pure logic such as the `errno`
  classification above.
- **Bridge.** It solves two hierarchies that vary independently (for example several protocols over several
  transports). There is one OS and one implementation per concept, so it would only add layers.
- **Adapters returning `std::unique_ptr<IFileDescriptor>` from `accept`.** The adapter would then own RAII and
  need the descriptor adapter too. Returning the raw fd keeps adapters stateless and ownership in one class.

## Open

- **Composition root:** `main.cpp` does not build the object graph yet. When it does, one function can create the
  four adapters and pass each class the interfaces it uses.
- **Config file read:** `AgentUserPolicy` still reads its config with `std::ifstream`. It is C++, not C, but the
  file read may want the same treatment later.
- **Non-blocking by default:** the `OPTIMIZE` in `systemd_activation.cpp` (make every `IFileDescriptor`
  non-blocking) is unchanged; `enableNonBlocking` now lives in `IDescriptorSyscalls`, which makes it easier.
