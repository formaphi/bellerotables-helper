# bellerotables-helper

C++23 firewall helper for the Bellerophon NGFW: a **one-shot** binary, activated by a systemd socket, that
applies iptables rules, **reads the real kernel state back** and returns that state to `bellero-agent`. It then
exits. It runs with `CAP_NET_ADMIN` only, not full root, and has no access to a database or the network.

It replaces using the iptables CLI and creating rules by hand. The target is the `meta-bellerophon` Yocto image
(Scarthgap, kernel 6.6). Development happens in a Docker sandbox (Ubuntu 24.04).

The rule that cannot be broken: `reported` comes from **reality** (a kernel readback), never an echo of
`desired`. If the apply is partial, `reported` shows exactly what stuck.

## Workflow

```
bellero-agent  ──(Unix socket)──▶  systemd  ──(sd_listen_fds)──▶  bellerotables-helper
                                                                      │ applies via libiptc
                                                                      ▼ reads the kernel back
bellero-agent  ◀──────────────────  reply (phase 2)  ─────────────────┘
```

1. systemd listens on `/run/bellerophon/bellerotables-helper.sock` (`Accept=no`, mode `0660`, the agent's group)
   and starts the helper when a connection arrives.
2. The helper takes the listening socket with `sd_listen_fds()` and `accept()`s **exactly one** connection.
   The others stay in the backlog for the next activation.
3. It checks the process on the other end with `SO_PEERCRED`: only the agent's user is accepted. The user comes
   from `/etc/bellerophon/bellerotables-helper.conf` (`agent_user=bellero-agent`); a missing file or an unknown
   user refuses every connection. The uid `PeerCredentials::UNKNOWN_ID` (0, an unread credential, which is also
   root) is always refused, so `agent_user=root` never authorizes anyone: the agent needs its own unprivileged
   user.
4. It reads one frame: 4 bytes of length (big-endian) followed by the payload. Frames above 64 KiB are refused
   before the payload is read. The payload is the canonical protobuf JSON `{correlation_id, id, desired}`.
5. It applies, reads the kernel state back and exits. Phase 1: no reply, the connection closes after handling.
   Phase 2: one reply frame on the same connection, then it closes.

### POSIX and system headers used

The `adapter_socket/` library only calls the C API behind interfaces (Adapter pattern). Each header below is
included only by the concrete adapter on its row; the domain classes receive the interface by injection and are
tested with mocks. The adapters return `SyscallResult<T>` / `std::error_code`, and the domain class decides
between retrying and throwing an exception. Details: [docs/syscall-adapters.md](docs/syscall-adapters.md).

| Header | Purpose | Interface ← adapter |
|--------|---------|---------------------|
| `<systemd/sd-daemon.h>` | `sd_listen_fds()`, `sd_is_socket_unix()`: receives the listening socket from systemd | `ISystemdDaemon` ← `LibsystemdDaemon` |
| `<sys/socket.h>` | `accept4`, `recv`, `send`, `getsockopt` (`SO_PEERCRED`) | `ISocketSyscalls` ← `PosixSocketSyscalls` |
| `<poll.h>`, `<unistd.h>`, `<fcntl.h>` | `poll` (wait with a deadline), `close`, `fcntl` (fd flags) | `IDescriptorSyscalls` ← `PosixDescriptorSyscalls` |
| `<pwd.h>`, `<unistd.h>` | `getpwnam_r`, `sysconf`: resolves the agent's user | `IUserDirectory` ← `PosixUserDirectory` |

Why a stream with a length prefix, and not UDP or ZeroMQ: [docs/socket-stream-and-framing.md](docs/socket-stream-and-framing.md).
Server and client roles: [docs/socket-roles.md](docs/socket-roles.md). The library's single entry point
(`acceptAuthorized()`): [docs/socket-endpoint.md](docs/socket-endpoint.md).

## Dependencies

| Dependency | Source | Use |
|------------|--------|-----|
| C++23, CMake 3.28+, GCC 13 | sandbox | same major version as Yocto Scarthgap |
| Conan 2.x | sandbox | manages the dependencies below |
| `nlohmann_json` 3.11.3 | Conan | payload JSON |
| `gtest` 1.14.0 (GTest + GMock) | Conan | tests |
| `libiptc-dev` (link `ip4tc`), `libxtables-dev` | apt | applying and reading rules |
| `libsystemd-dev` | apt | socket activation |
| `iptables` on the legacy backend | apt | the CLI sees what libiptc writes |

Adding a Conan dependency requires running `make sandbox-build` again: the image pre-builds the cache and
`conan install` runs with `--no-remote`.

## Make commands

Every build runs inside the sandbox. Run `make help` for the full list.

```bash
make setup                            # activates the pre-commit hook (once after cloning)
make sandbox-build                    # builds the image (pre-builds the Conan deps)
make sandbox-shell                    # shell in the container, repo at /workspace

make rebuild                          # Release binary -> production-builds/release/
make rebuild-unit && make test-unit   # tests + coverage (coverage-reports/html/index.html)
make rebuild-asan && make test-asan   # tests under ASan + UBSan
make rebuild-valgrind && make test-valgrind
make rebuild-tsan && make test-tsan   # see the note below
make test-one-asan PATTERN=<regex>    # one test binary; also works for -unit, -tsan, -valgrind

make format                           # applies clang-format (format-check only checks)
make tidy-fix                         # applies clang-tidy (tidy-check only checks); needs 'make configure'
```

TSan aborts with `unexpected memory mapping` on hosts with `vm.mmap_rnd_bits=32`. Run
`sudo sysctl vm.mmap_rnd_bits=28` on the host before `make test-tsan`.

The pre-commit hook runs: sandbox image, `format-check`, release, `tidy-check`, unit tests, ASan and Valgrind.
How each build type works and its pitfalls: [docs/build/](docs/build/).

## Commit convention

[Conventional Commits](https://www.conventionalcommits.org/) in English: `type(scope): description`, with the
scope optional.

```
feat(adapter_socket): socket endpoint, listener, deadline and file descriptor
build: port piiptables sandbox build infra
docs: record perf profiling options, deferred until a high-load E2E exists
```

Types used: `feat`, `fix`, `refactor`, `test`, `build`, `docs`, `chore`.

Branch flow: a feature branches off `develop` and comes back by squash merge. `develop` only enters `master`
after the slice passes the end-to-end validation in `EndToEnd`. A branch is only deleted after its work has
reached `master`.

## Include convention

First the file's own header; then project headers with quotes (`#include "adapter_socket/..."`); finally
everything from outside the project with `<>` (C/POSIX, standard library, gtest, libsystemd). Each block in
alphabetical order. `make format` applies the ordering; `make format-check` also rejects a project header
included with `<>`. Details: [docs/build/clang-format.md](docs/build/clang-format.md).

## Comment convention

Every comment starts with an uppercase **modifier**, followed by a colon and a description. A comment without a
modifier is not accepted, and neither is a bare modifier.

| Modifier | Meaning |
|----------|---------|
| `BUG` | known issue that causes incorrect behavior |
| `COMPLEX` | complicated code that needs review or refactoring |
| `FIXME` | known issue that needs to be fixed |
| `HACK` | solution that is not ideal or is a temporary workaround |
| `LEARN` | important, non-obvious point that needs understanding (use it to explain code) |
| `OPTIMIZE` | code that can be optimized |
| `REVIEW` | code that deserves review |
| `TODO` | more work to be done in this block |

```cpp
// LEARN: SO_PEERCRED returns the uid of the process that called connect(), not of whoever opened the socket.
// TODO: handle EINTR in poll once signal handling exists.
```

These are the [Todo Tree](https://github.com/Gruntfuggly/todo-tree) tags, which lists all of them in the
editor. They stay in the code when merging, so do not remove them beforehand.
