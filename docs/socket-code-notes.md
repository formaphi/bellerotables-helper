# Code annotations in `adapter_socket`

**Status:** recorded 2026-10-28.
**Why this file exists:** `CLAUDE.md` allows Todo Tree annotations (`TODO:`, `FIXME:`, `OPTIMIZE:`, `LEARN:`) and
no other comments. Four `LEARN` / `OPTIMIZE` blocks and a few plain `errno` explanations are in the sources. This
file records what each one says and where the follow-up lives, so a block can be removed from the code without
losing the idea. The plain `errno` explanations are not Todo Tree annotations and should become names or move here.

## `OPTIMIZE` blocks (follow-ups)

| Where | Idea | Follow-up |
|-------|------|-----------|
| ~~`unix_socket_connection.cpp`, `readExactly`~~ | ~~The `errno` handling should not be repeated inline.~~ | Done: `socket_syscalls/retryable_error.h` and `ISocketSyscalls` ([syscall-adapters.md](syscall-adapters.md)). The block was removed. |
| `systemd_activation.cpp`, `takeListeningFd` | Move the non-blocking switch (`IDescriptorSyscalls::enableNonBlocking`) into the file descriptor, so activation does not own this and every `IFileDescriptor` is non-blocking by default. | Not planned yet. It changes the contract of `IFileDescriptor` ([socket-file-descriptor.md](socket-file-descriptor.md)). |

## `LEARN` blocks (explanations)

| Where | What it explains |
|-------|------------------|
| `posix_descriptor_syscalls.cpp`, after `::poll` | `poll` suspends the thread until the fd is ready for the requested operation or the timeout expires. Here it waits for one fd to become readable or writable, depending on `Readiness`. |
| `posix_descriptor_syscalls.cpp`, `enableNonBlocking` | `fcntl` reads and changes the file status flags of an fd. Here it turns on non-blocking mode. |
| `posix_socket_syscalls.cpp`, `sendSome` | `MSG_DONTWAIT` keeps each `send` from blocking past the deadline, because the accepted fd is blocking and `POLLOUT` does not guarantee room for every byte. `MSG_NOSIGNAL` turns a write to a closed peer into `EPIPE` instead of a `SIGPIPE` that kills the process. |
| `posix_socket_syscalls.cpp`, `readPeerCredentials` | What `SOL_SOCKET` / `SO_PEERCRED` / `ucred` mean and why the kernel-recorded uid cannot be forged. |
| `posix_user_directory.cpp`, `findUserId` | Why `getpwnam_r` is retried with a doubled buffer on `ERANGE`. |
| `libsystemd_daemon.cpp`, `takeListenFds` | How `sd_listen_fds` numbers the fds from `SD_LISTEN_FDS_START` and why it unsets the environment. |
| `syscall_result/syscall_result.h` | Why adapters return `SyscallResult<T>` instead of `std::expected`. |

## Explanatory comments about `errno`

These explain why a failed syscall is retried instead of reported:

| Where | Errors | Why retrying is safe |
|-------|--------|----------------------|
| `socket_syscalls/retryable_error.h` (used by `readExactly`, `writeAll`, `acceptOne`) | `EAGAIN`, `EWOULDBLOCK`, `EINTR`, and `ECONNABORTED` for accept | No data moved and no connection was taken, so polling again before the same deadline is safe. Now a `LEARN` block. |
| `file_descriptor.cpp`, `waitUntilReady` | poll timeout, `EINTR` | A timeout on a millisecond boundary lets the loop head decide whether the deadline really passed. A signal interrupted `poll` before it reported readiness. |
| `steady_deadline.cpp`, `pollTimeoutMilliseconds` | none | The poll timeout is rounded up (ceil). A truncated timeout wakes before the deadline and the loop spins. |

`EAGAIN` and `EWOULDBLOCK` are the same value on Linux but may differ elsewhere, so the code tests both.
