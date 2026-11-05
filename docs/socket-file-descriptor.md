# File descriptors in `adapter_socket`

**Status:** decided and implemented 2026-10-23.
**Origin:** items 3 and 5 of the review in [socket-review.md](socket-review.md) (Versão 2 and 3).

## Decision

- **`FileDescriptor : IFileDescriptor` owns every fd.** It closes the fd in its destructor and cannot be copied
  or moved, like every other concrete class.
- **Ownership moves as `std::unique_ptr<IFileDescriptor>`:**

  ```
  SystemdActivation::takeListeningFd()   → fd 3, owned from the moment it is validated
  UnixSocketListenerFactory::create(fd)  → UnixSocketListener owns fd 3
  UnixSocketListener::acceptOne()        → UnixSocketConnection owns the accepted fd
  ```

- **Listener and connection hold the fd as a member.** When their constructor throws, the member is already
  built, so its destructor closes the fd. There is no `closeFd()`, `INVALID_FD` or constructor `try/catch`.
- **The fd waits for itself.** `waitUntilReady(Readiness, deadline)` runs the single `poll()` bounded by a
  `steady_clock` deadline and throws `std::system_error(ETIMEDOUT, "<operation> deadline exceeded")`.
  `DeadlinePoller` no longer exists.
- **`Readiness` names the operation.** `Readable` → `"read"`, `Writable` → `"write"`, `Acceptable` → `"accept"`,
  mapped with `NLOHMANN_JSON_SERIALIZE_ENUM` (`file_descriptor/readiness.h`). Callers no longer pass the operation
  text. `Acceptable` polls `POLLIN` like `Readable`; it exists so an accept timeout is reported as "accept".
- **`value()` is the only raw access**, passed to the syscall adapters (`ISocketSyscalls` for `accept4`, `recv`,
  `send`, `getsockopt`). This is the exception `cpp-wrapping-c-pure-posix` makes for exposing the fd.
- **`close` and `poll` go through `IDescriptorSyscalls`.** `FileDescriptor` keeps the ownership and the deadline
  loop; the C calls live in `PosixDescriptorSyscalls`. See [syscall-adapters.md](syscall-adapters.md).
- **Peer credentials need no DTO.** `UnixSocketListener` reads them right after `accept4` through
  `ISocketSyscalls::readPeerCredentials`, which converts `ucred` with `PeerCredentials::fromUcred`.

## Rejected

- **Move-only `FileDescriptor` held by value.** It breaks the rule that every concrete class inherits an interface,
  and it cannot be mocked. `unique_ptr` gives the same move-only transfer.
- **`AcceptedSocketDto { unique_ptr<IFileDescriptor>, PeerCredentials }` built by the listener.** It would only
  carry data across one constructor call, and the listener would have to know about credentials.
- **Credentials and deadline in one DTO.** The credentials are read once per connection. The deadline is
  recomputed for every operation.

## Open

- **`Deadline` as its own type:** decided in [socket-deadline.md](socket-deadline.md). The
  `steady_clock::time_point` parameter of `waitUntilReady` becomes a `const IDeadline&`.
