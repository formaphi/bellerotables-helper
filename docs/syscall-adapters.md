# Syscall adapters in `adapter_socket`

**Status:** proposed 2026-10-28, not implemented.
**Origin:** discussion of where the raw POSIX calls (`getpwnam_r`, `getsockopt`, `recv`, `send`, `accept4`)
should live.

## Problem

POSIX calls are scattered across the classes that hold domain logic:

| File | Calls |
|------|-------|
| `file_descriptor.cpp` | `close`, `poll` (already behind `IFileDescriptor`) |
| `unix_socket_listener.cpp` | `accept4`, `getsockopt` |
| `unix_socket_connection.cpp` | `recv`, `send` |
| `agent_user_policy.cpp` | `sysconf`, `getpwnam_r` and the `ERANGE` retry loop |

`AgentUserPolicy` is the worst case: it reads a file, interprets the config and talks to the OS user database. The
`ERANGE` loop and the "user does not exist" path can only be tested with a real user on the machine.

## Decision

- **One small interface per kind of syscall, not one interface for all of them.** Each interface is an Adapter: it
  turns a C API (`errno`, raw pointers, caller-owned buffers) into an operation the rest of the code can call and
  mock.

  ```
  IFileDescriptor   ← FileDescriptor      (exists)
  IUserDirectory    ← PosixUserDirectory  (first to extract)
  ISocketSyscalls   ← PosixSocketSyscalls (only if socket failures need simulating)
  ```

- **The adapter absorbs the C details and exposes a domain operation.** `IUserDirectory::findUserId(name)` hides
  `sysconf`, `getpwnam_r`, the buffer and the `ERANGE` retry, and returns `std::optional<uint32_t>`.
  `AgentUserPolicy` receives an `IUserDirectory` and keeps only reading the config and comparing the uid.
- **Same conventions as the rest of the lib:** an `interface_*.h` header, a concrete `Posix*` class, a
  `mock/mock_*.h`, one folder per concept under `include/adapter_socket/`.
- **Add an interface only where there is logic worth isolating** or a failure that cannot be provoked with a real
  `socketpair()` (`ECONNABORTED`, `EINTR` in the middle of `recv`). A one-line syscall that tests already exercise
  with a real socket stays where it is.
- **Pure helpers need no interface.** Classifying an `errno` as retryable is repeated in `recv`, `send` and
  `accept4` (the last also retries `ECONNABORTED`). Two small functions in an internal header replace the
  repeated expressions.
- **Wiring is composition at the root.** If a single place should hold every adapter, it is a plain struct of
  `std::unique_ptr` to the interfaces, built by one function. Classes never receive that struct, only the
  interface they use.

## Rejected

- **`ISystem` with every syscall.** It breaks interface segregation (`AgentUserPolicy` would depend on `recv` and
  `accept4`), needs a large mock per test, grows with every new call, and only forwards the C functions 1:1, so the
  retry loops and error handling stay in the callers.
- **A `posix_utils` of free functions for OS calls.** The dependency is hidden and cannot be replaced by a mock,
  against the injection used everywhere else. Free functions are fine only for pure logic such as the `errno`
  classification above.
- **Bridge.** It solves two hierarchies that vary independently (for example several protocols over several
  transports). There is one OS and one implementation per concept, so it would only add layers.

## Open

- **`ISocketSyscalls` scope:** extract it only when a test needs to simulate a socket failure that `socketpair()`
  cannot produce. Until then `recv`, `send`, `accept4` and `getsockopt` stay with their owners.
- **`IUserDirectory` constructor injection:** `AgentUserPolicy` also reads its config file, so the file read may
  want the same treatment later.
