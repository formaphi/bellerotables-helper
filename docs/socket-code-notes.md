# Code annotations in `adapter_socket`

**Status:** recorded 2026-10-28.
**Why this file exists:** `CLAUDE.md` allows Todo Tree annotations (`TODO:`, `FIXME:`, `OPTIMIZE:`, `LEARN:`) and
no other comments. Four `LEARN` / `OPTIMIZE` blocks and a few plain `errno` explanations are in the sources. This
file records what each one says and where the follow-up lives, so a block can be removed from the code without
losing the idea. The plain `errno` explanations are not Todo Tree annotations and should become names or move here.

## Uncommitted change

`adapter_socket/src/unix_socket_connection.cpp`: an `OPTIMIZE` block added before the `EAGAIN` / `EWOULDBLOCK`
check in `readExactly`. No behavior change.

## `OPTIMIZE` blocks (follow-ups)

| Where | Idea | Follow-up |
|-------|------|-----------|
| `unix_socket_connection.cpp`, `readExactly` | The `errno` handling should not be repeated inline. Interpret each syscall result in one place instead of in every caller. | [syscall-adapters.md](syscall-adapters.md): classify `errno` with small internal helpers, and add `ISocketSyscalls` only if a socket failure must be simulated. |
| `systemd_activation.cpp`, `enableNonBlocking` | Move the two `fcntl` calls (`F_GETFL`, `F_SETFL` with `O_NONBLOCK`) into the file descriptor, so activation does not own this and every `IFileDescriptor` is non-blocking by default. | Not planned yet. It changes the contract of `IFileDescriptor` ([socket-file-descriptor.md](socket-file-descriptor.md)). |

## `LEARN` blocks (explanations)

| Where | What it explains |
|-------|------------------|
| `file_descriptor.cpp`, after `::poll` | `poll` suspends the thread until the fd is ready for the requested operation or the timeout expires. Here it waits for one fd to become readable or writable, depending on `Readiness`. |
| `systemd_activation.cpp`, `enableNonBlocking` | `fcntl` reads and changes the file status flags of an fd. Here it turns on non-blocking mode. |

## Explanatory comments about `errno`

These explain why a failed syscall is retried instead of reported:

| Where | Errors | Why retrying is safe |
|-------|--------|----------------------|
| `unix_socket_connection.cpp`, `readExactly` and `writeAll` | `EINTR` | A signal interrupted `recv` / `send` before any data moved. |
| `unix_socket_listener.cpp`, `acceptOne` | `EAGAIN`, `EINTR`, `ECONNABORTED` | No connection was taken, so polling again before the same deadline is safe. |
| `file_descriptor.cpp`, `waitUntilReady` | poll timeout, `EINTR` | A timeout on a millisecond boundary lets the loop head decide whether the deadline really passed. A signal interrupted `poll` before it reported readiness. |
| `steady_deadline.cpp`, `pollTimeoutMilliseconds` | none | The poll timeout is rounded up (ceil). A truncated timeout wakes before the deadline and the loop spins. |

`EAGAIN` and `EWOULDBLOCK` are the same value on Linux but may differ elsewhere, so the code tests both.
