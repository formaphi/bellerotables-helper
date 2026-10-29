# Socket Review V4 Implementation Plan

> **For agentic workers:** execute this plan task by task, in order. Steps use checkbox (`- [ ]`) syntax for
> tracking. Do not commit (repo rule: commit only when the user asks).

**Goal:** apply the "Respostas à Versão 4" of `docs/socket-review.md` to `adapter_socket/`.

**Architecture:** whoever defines "one operation" opens its deadline. `SocketEndpoint` owns the
`IDeadlineFactory` + a validated `SocketTimeoutsDto`, opens the accept deadline and passes it to
`ISocketListener::acceptOne(const IDeadline&)`. The listener accepts, reads `SO_PEERCRED` and returns
`AcceptedConnectionDto { connection, peer }`. `FramedConnection` opens one deadline per frame and passes it to
`ISocketConnection::readExactly/writeAll`. `UnixSocketConnection`, `UnixSocketListener` and
`UnixSocketListenerFactory` end up holding only the fd.

**Tech stack:** C++23, CMake, GTest/GMock, all builds inside the sandbox via `make` (repo root
`/home/mrcooper/development/hub-bellerophon/bellerotables-helper`).

---

## House rules (every task)

- No comments or docstrings in code. Names explain.
- No free functions. Helpers are `static` private members. File-scope `constexpr` constants are fine.
- Headers only under `adapter_socket/include/adapter_socket/<area>/`. Sources in `adapter_socket/src/`, tests in
  `adapter_socket/test/` (globbed: new files are picked up automatically, no CMake edit).
- Interfaces: only a `virtual ~I...() = default;` and pure virtuals. No constructors.
- Every concrete class is `final`, inherits an interface, deletes copy and move. DTOs (`struct ...Dto`) are
  exempt: plain data + static `create`/`parse` helpers, like `framed_connection/frame_header_dto.h`.
- No calls and no compound expressions inside `if`/`while` conditions: put the result in a named `const bool`
  or `const int` first (see `src/unix_socket_listener.cpp`).
- No magic numbers: named `constexpr` constants.
- Mirror the include order, brace style and naming (`m_member`, `camelCase`) of the surrounding files.

## Build / test commands (from the repo root)

- Unit tests: `make rebuild-unit && make test-unit`
- One test binary filter: `make test-one-unit PATTERN=<regex>`
- Sanitizers: `make rebuild-asan && make test-asan`
- Lint: `make format` (rewrites), then `make format-check` and `make tidy-check`

If `make` fails because the sandbox image is missing, run `make sandbox-build` once.

---

### Task 1: Drop the JSON enum mapping from `Readiness` / `FileDescriptor`

**Files:**
- Modify: `adapter_socket/include/adapter_socket/file_descriptor/readiness.h`
- Modify: `adapter_socket/include/adapter_socket/file_descriptor/file_descriptor.h`
- Modify: `adapter_socket/src/file_descriptor.cpp`
- Modify: `adapter_socket/test/file_descriptor_test.cpp`
- Delete: `adapter_socket/test/readiness_test.cpp`

- [ ] **Step 1: Update the test first.** In `file_descriptor_test.cpp`, replace the constants
  `READ_DEADLINE_MESSAGE` and `ACCEPT_DEADLINE_MESSAGE` with one constant
  `constexpr const char* DEADLINE_EXCEEDED_MESSAGE = "poll deadline exceeded";` and use it in both
  `expectDeadlineExceeded(...)` calls (around lines 154 and 161). Delete `readiness_test.cpp`.
- [ ] **Step 2: Run** `make rebuild-unit && make test-one-unit PATTERN=FileDescriptor`. Expected: FAIL (the
  message is still "read deadline exceeded").
- [ ] **Step 3: Implement.**
  - `readiness.h`: remove `#include <nlohmann/json.hpp>` and the `NLOHMANN_JSON_SERIALIZE_ENUM` block. Keep the
    enum with its three values.
  - `file_descriptor.h`: remove `operationName` and `#include <string>`.
  - `file_descriptor.cpp`: remove `using json = ...`, `operationName`, the `timeoutMessage` local. Add a file
    constant `constexpr const char* DEADLINE_EXCEEDED_MESSAGE = "poll deadline exceeded";` and throw
    `std::system_error(ETIMEDOUT, std::generic_category(), DEADLINE_EXCEEDED_MESSAGE)`.
- [ ] **Step 4: Run** `make rebuild-unit && make test-unit`. Expected: all PASS.

---

### Task 2: Make the activated listening fd non-blocking

Reason: systemd hands the fd over blocking; if the client aborts between `poll` and `accept4`, `accept4` blocks
with no deadline. `EAGAIN` is already retried in `UnixSocketListener::acceptOne`.

**Files:**
- Modify: `adapter_socket/src/systemd_activation.cpp`
- Modify: `adapter_socket/test/systemd_activation_test.cpp`

- [ ] **Step 1: Write the failing test.** Read `systemd_activation_test.cpp` and copy the setup of the existing
  test that returns a valid listening fd (the one that dups an abstract listening socket onto
  `SD_LISTEN_FDS_START` and sets `LISTEN_PID`/`LISTEN_FDS`). Add a test
  `TakenListeningFdIsNonBlocking`: after `takeListeningFd()`, read `::fcntl(fd->value(), F_GETFL)` into a
  `const int` and assert `(flags & O_NONBLOCK) != 0` (via a named `const bool isNonBlocking`). Add
  `#include <fcntl.h>`.
- [ ] **Step 2: Run** `make rebuild-unit && make test-one-unit PATTERN=SystemdActivation`. Expected: the new test
  FAILS.
- [ ] **Step 3: Implement.** In `SystemdActivation`, add a private static
  `static void enableNonBlocking(int fileDescriptor);` (declare it in
  `include/adapter_socket/activation/systemd_activation.h`). It calls `::fcntl(fd, F_GETFL)`; on `-1` throws
  `std::system_error(errno, std::generic_category(), "fcntl F_GETFL")`; then
  `::fcntl(fd, F_SETFL, flags | O_NONBLOCK)`; on `-1` throws with `"fcntl F_SETFL"`. Save `errno` into
  `const int savedErrno` before throwing, like the rest of the file. Call it in `takeListeningFd()` after the
  `sd_is_socket_unix` checks, before wrapping in `FileDescriptor`.
- [ ] **Step 4: Run** `make rebuild-unit && make test-unit`. Expected: all PASS.

---

### Task 3: `SocketTimeoutsDto`

**Files:**
- Create: `adapter_socket/include/adapter_socket/socket_endpoint/socket_timeouts_dto.h`
- Create: `adapter_socket/src/socket_timeouts_dto.cpp`
- Create: `adapter_socket/test/socket_timeouts_dto_test.cpp`

- [ ] **Step 1: Write the failing tests** in `socket_timeouts_dto_test.cpp` (style of
  `frame_header_dto_test.cpp`, plain `TEST`):
  - `CreateKeepsPositiveTimeouts`: `create(250ms, 750ms)` → fields equal those values.
  - `DefaultsArePositive`: a default-constructed DTO has both fields equal to `DEFAULT_ACCEPT_TIMEOUT` /
    `DEFAULT_CONNECTION_TIMEOUT` and both `> 0ms`.
  - `CreateRejectsZeroAcceptTimeout` and `CreateRejectsZeroConnectionTimeout`: `EXPECT_THROW(..., std::invalid_argument)`.
  - `CreateRejectsNegativeTimeout`: `create(-1ms, 750ms)` throws.
- [ ] **Step 2: Run** `make rebuild-unit`. Expected: compile FAIL (header missing).
- [ ] **Step 3: Implement.**

```cpp
#ifndef ADAPTER_SOCKET_SOCKET_TIMEOUTS_DTO_H
#define ADAPTER_SOCKET_SOCKET_TIMEOUTS_DTO_H

#include <chrono>

namespace adapter_socket {

struct SocketTimeoutsDto
{
    static constexpr std::chrono::milliseconds DEFAULT_ACCEPT_TIMEOUT = std::chrono::milliseconds(5000);
    static constexpr std::chrono::milliseconds DEFAULT_CONNECTION_TIMEOUT =
        std::chrono::milliseconds(5000);

    std::chrono::milliseconds acceptTimeout = DEFAULT_ACCEPT_TIMEOUT;
    std::chrono::milliseconds connectionTimeout = DEFAULT_CONNECTION_TIMEOUT;

    [[nodiscard]] static SocketTimeoutsDto create(std::chrono::milliseconds acceptTimeout,
                                                  std::chrono::milliseconds connectionTimeout);
};

} // namespace adapter_socket

#endif // ADAPTER_SOCKET_SOCKET_TIMEOUTS_DTO_H
```

  `create` in the `.cpp`: file constant `constexpr std::chrono::milliseconds NO_TIMEOUT(0);`,
  `const bool hasInvalidTimeout = acceptTimeout <= NO_TIMEOUT || connectionTimeout <= NO_TIMEOUT;`, throw
  `std::invalid_argument("accept and connection timeouts must be greater than zero")`, else return the filled DTO.
- [ ] **Step 4: Run** `make rebuild-unit && make test-unit`. Expected: all PASS.

---

### Task 4: Remove the dead `configureTimeouts`

`SO_RCVTIMEO`/`SO_SNDTIMEO` never apply: every `recv`/`send` uses `MSG_DONTWAIT` after `poll`.

**Files:**
- Modify: `adapter_socket/include/adapter_socket/socket_connection/unix_socket_connection.h`
- Modify: `adapter_socket/src/unix_socket_connection.cpp`

- [ ] **Step 1:** Delete `configureTimeouts` (declaration, definition, call in the constructor) and the now
  unused `#include <sys/time.h>`.
- [ ] **Step 2: Run** `make rebuild-unit && make test-unit`. Expected: all PASS (no behaviour change; the
  timeout tests rely on the deadline, not on the socket options).

---

### Task 5: Deadlines opened by the caller, peer credentials returned by the accept

This task changes interfaces that cascade, so the build is only green again at Step 9. Do all steps, then build.

**Files:**
- Modify: `include/adapter_socket/socket_connection/interface_socket_connection.h`,
  `include/adapter_socket/socket_connection/unix_socket_connection.h`, `src/unix_socket_connection.cpp`
- Create: `include/adapter_socket/socket_listener/accepted_connection_dto.h`
- Modify: `include/adapter_socket/socket_listener/interface_socket_listener.h`,
  `include/adapter_socket/socket_listener/unix_socket_listener.h`, `src/unix_socket_listener.cpp`
- Modify: `include/adapter_socket/socket_listener/unix_socket_listener_factory.h`,
  `src/unix_socket_listener_factory.cpp`
- Modify: `include/adapter_socket/framed_connection/framed_connection.h`, `src/framed_connection.cpp`
- Modify: `include/adapter_socket/socket_endpoint/socket_endpoint.h`, `src/socket_endpoint.cpp`
- Modify mocks: `include/adapter_socket/mock/mock_socket_connection.h`, `mock/mock_socket_listener.h`
- Modify tests: `unix_socket_connection_test.cpp`, `unix_socket_listener_test.cpp`,
  `unix_socket_listener_factory_test.cpp`, `framed_connection_test.cpp`, `socket_endpoint_test.cpp`

(all paths under `adapter_socket/`)

- [ ] **Step 1: `ISocketConnection`.** New contract:

```cpp
virtual std::vector<uint8_t> readExactly(std::size_t byteCount, const IDeadline& deadline) = 0;
virtual void writeAll(const std::vector<uint8_t>& bytes, const IDeadline& deadline) = 0;
```

  Remove `peerCredentials()` and the `peer_credentials.h` include; include `deadline/interface_deadline.h`.

- [ ] **Step 2: `UnixSocketConnection`.** Constructor becomes
  `explicit UnixSocketConnection(std::unique_ptr<IFileDescriptor> connectedFd);` (body empty, only the member
  init). Remove `DEFAULT_TIMEOUT`, `m_deadlineFactory`, `m_timeout`, `m_peerCredentials`, `peerCredentials()`,
  `readPeerCredentials()`, the timeout validation, `NO_TIMEOUT`, and the now unused includes (`<chrono>`,
  `<stdexcept>`, `peer_credentials.h`, `interface_deadline_factory.h`). `readExactly`/`writeAll` drop the
  `startingNow` line and pass the parameter `deadline` to `waitUntilReady`. Loop bodies otherwise unchanged.

- [ ] **Step 3: `AcceptedConnectionDto`** (new header, no `.cpp`):

```cpp
#ifndef ADAPTER_SOCKET_ACCEPTED_CONNECTION_DTO_H
#define ADAPTER_SOCKET_ACCEPTED_CONNECTION_DTO_H

#include <adapter_socket/peer_connection/peer_credentials.h>
#include <adapter_socket/socket_connection/interface_socket_connection.h>

#include <memory>

namespace adapter_socket {

struct AcceptedConnectionDto
{
    std::shared_ptr<ISocketConnection> connection;
    PeerCredentials peer;
};

} // namespace adapter_socket

#endif // ADAPTER_SOCKET_ACCEPTED_CONNECTION_DTO_H
```

- [ ] **Step 4: `ISocketListener` / `UnixSocketListener`.**
  - Interface: `virtual AcceptedConnectionDto acceptOne(const IDeadline& deadline) = 0;` (include the DTO and
    `interface_deadline.h`).
  - Constructor: `explicit UnixSocketListener(std::unique_ptr<IFileDescriptor> listeningFd);`. Remove
    `DEFAULT_ACCEPT_TIMEOUT`, `m_deadlineFactory`, `m_acceptTimeout`, `m_connectionTimeout`, the validation,
    `NO_TIMEOUT`, `<stdexcept>`, `<chrono>`.
  - Add `[[nodiscard]] static PeerCredentials readPeerCredentials(const IFileDescriptor& connectedFd);` as a
    private member. Move the body of the old `UnixSocketConnection::readPeerCredentials` here
    (`getsockopt(SO_PEERCRED)` → `PeerCredentials::fromUcred`), using `connectedFd.value()`.
  - `acceptOne(const IDeadline& deadline)`: same loop, `waitUntilReady(Readiness::Acceptable, deadline)`. On
    success: build `std::unique_ptr<IFileDescriptor> acceptedFd = std::make_unique<FileDescriptor>(accepted);`,
    then `const PeerCredentials peer = readPeerCredentials(*acceptedFd);`, then
    `AcceptedConnectionDto result;` `result.peer = peer;`
    `result.connection = std::make_shared<UnixSocketConnection>(std::move(acceptedFd));` and return it.
    (Read the credentials before the move. If `getsockopt` throws, `acceptedFd` closes the fd.)

- [ ] **Step 5: `UnixSocketListenerFactory`.** No constructor parameters and no members:
  `UnixSocketListenerFactory() = default;` (keep `~... override = default` and the deleted copy/move).
  `create` returns `std::make_shared<UnixSocketListener>(std::move(listeningFd));`. Remove `NO_TIMEOUT`,
  validation and unused includes in the `.cpp`/`.h`.

- [ ] **Step 6: `FramedConnection`.** Constructor:

```cpp
FramedConnection(std::shared_ptr<ISocketConnection> connection,
                 std::shared_ptr<IDeadlineFactory> deadlineFactory,
                 std::chrono::milliseconds frameTimeout);
```

  Members `m_deadlineFactory`, `m_frameTimeout`. `readFrame`: open **one** deadline
  `const std::unique_ptr<IDeadline> deadline = m_deadlineFactory->startingNow(m_frameTimeout);` at the top and
  pass `*deadline` to both `readExactly` calls. `writeFrame`: same, one deadline, opened **after** the
  too-large check, passed to `writeAll`. Drop `explicit` (3 parameters).

- [ ] **Step 7: `SocketEndpoint`.** Constructor:

```cpp
SocketEndpoint(const std::shared_ptr<ISocketActivation>& activation,
               const std::shared_ptr<ISocketListenerFactory>& listenerFactory,
               std::shared_ptr<IPeerPolicy> peerPolicy,
               std::shared_ptr<IDeadlineFactory> deadlineFactory,
               const SocketTimeoutsDto& timeouts = SocketTimeoutsDto());
```

  Members `m_deadlineFactory`, `m_timeouts`. `acceptAuthorized()`:

```cpp
const std::unique_ptr<IDeadline> acceptDeadline =
    m_deadlineFactory->startingNow(m_timeouts.acceptTimeout);
const AcceptedConnectionDto accepted = m_listener->acceptOne(*acceptDeadline);
const bool isAuthorized = m_peerPolicy->isAllowed(accepted.peer);
if (!isAuthorized)
{
    return std::nullopt;
}
return std::make_shared<FramedConnection>(accepted.connection, m_deadlineFactory,
                                          m_timeouts.connectionTimeout);
```

- [ ] **Step 8: Mocks.**
  - `MockSocketConnection`: `readExactly(std::size_t byteCount, const IDeadline& deadline)`,
    `writeAll(const std::vector<uint8_t>& bytes, const IDeadline& deadline)`; remove `peerCredentials`.
  - `MockSocketListener`: `MOCK_METHOD(AcceptedConnectionDto, acceptOne, (const IDeadline& deadline), (override));`

- [ ] **Step 9: Tests.** Adapt, then add the new ones.
  - `unix_socket_connection_test.cpp`:
    - construct with `UnixSocketConnection connection(std::move(localFd));`;
    - every call passes a deadline: `const std::unique_ptr<IDeadline> deadline = m_deadlines->startingNow(<timeout>);`
      then `connection.readExactly(n, *deadline)`. Use a file constant
      `constexpr std::chrono::milliseconds DEFAULT_TEST_TIMEOUT(5000);` where no timeout was given, and the
      existing `IDLE_READ_TIMEOUT` / `SLOW_LORIS_TIMEOUT` / `BLOCKED_WRITE_TIMEOUT` where they were;
    - `IdlePeerReadTimesOutWithEtimedout`: pass `*expiredDeadline()` directly; drop the `MockDeadlineFactory`;
    - in `LargeWriteCompletesWhileReaderDrains`, create the deadline before the thread and capture it by reference;
    - delete `ZeroTimeoutIsRejectedAndTheDescriptorIsClosed` and `PeerCredentialsReportCurrentUserId`
      (both concerns moved); remove unused constants/includes (`ZERO_TIMEOUT`, `MockDeadlineFactory`, `<stdexcept>`).
  - `unix_socket_listener_test.cpp`:
    - `UnixSocketListener listener(std::move(ownedListeningFd));`, client side
      `UnixSocketConnection(std::move(ownedClientFd))`;
    - `AcceptsPendingClientAndReportsPeerCredentials`: `const AcceptedConnectionDto accepted = listener.acceptOne(*deadline);`
      assert `accepted.connection` non-null and `accepted.peer.userId == getuid()`;
    - `AcceptedConnectionCarriesFramedPayloadFromClient`: both `FramedConnection`s get
      `(connection, deadlineFactory, FRAME_TIMEOUT)` with `constexpr std::chrono::milliseconds FRAME_TIMEOUT(5000);`;
      the server uses `accepted.connection`;
    - `AcceptWithoutPendingClientTimesOut`: `listener.acceptOne(*expiredDeadline())`, no factory;
    - delete `RejectedAcceptTimeoutClosesListeningFd` and `RejectedConnectionTimeoutClosesListeningFd`
      (validation now lives in `SocketTimeoutsDto`, tested in Task 3) and the then-unused
      `secondSocketPathIn`/`SECOND_SOCKET_FILE_NAME` if nothing else uses them.
  - `unix_socket_listener_factory_test.cpp`: keep one test `CreatesListener` with
    `const adapter_socket::UnixSocketListenerFactory factory;`; delete the timeout tests and unused constants.
  - `framed_connection_test.cpp`:
    - the fakes implement the new signatures (ignore the deadline parameter: leave it unnamed
      `const adapter_socket::IDeadline&`) and drop `peerCredentials` and the credentials member/ctor param;
    - every `FramedConnection` is built with `(side, deadlineFactory, FRAME_TIMEOUT)`; use a fixture-less helper:
      a file constant `FRAME_TIMEOUT` and a `std::make_shared<adapter_socket::SteadyDeadlineFactory>()`;
    - **new** `ReadFrameOpensOneDeadlineForHeaderAndPayload`: a `MockDeadlineFactory` with
      `EXPECT_CALL(*factory, startingNow(FRAME_TIMEOUT)).Times(1).WillOnce(...)` returning a `MockDeadline`
      (`hasExpired` → false is enough; the fake never polls), reader over `THREE_BYTE_FRAME`, `readFrame()` returns
      `THREE_BYTE_PAYLOAD`;
    - **new** `WriteFrameOpensOneDeadline`: same expectation with `Times(1)` around `writeFrame`;
    - **new** `WriteFrameAboveMaximumOpensNoDeadline`: `startingNow` `.Times(0)`, too-large payload throws.
  - `socket_endpoint_test.cpp`:
    - fixture gets `std::shared_ptr<MockDeadlineFactory> m_deadlineFactory` (default action: return a
      `MockDeadline`; use `ON_CALL(...).WillByDefault([](std::chrono::milliseconds) { return std::make_unique<MockDeadline>(); })`
      in `SetUp`, or `EXPECT_CALL` per test) and `buildEndpoint()` passes it plus
      `SocketTimeoutsDto::create(ACCEPT_TIMEOUT, CONNECTION_TIMEOUT)` with two distinct constants (e.g. 250ms, 750ms);
    - `acceptOne(_)` now returns `AcceptedConnectionDto{m_connection, STRANGER_PEER}` /
      `{m_connection, AGENT_PEER}`; remove the `peerCredentials()` expectations;
    - **new** `AcceptDeadlineUsesConfiguredAcceptTimeout`: `EXPECT_CALL(*m_deadlineFactory, startingNow(ACCEPT_TIMEOUT)).Times(1)`
      and the peer refused (so no frame deadline is opened).
- [ ] **Step 10: Run** `make rebuild-unit && make test-unit`. Expected: all PASS. If a test fails, fix the code
  or the adapted test; do not delete a test that is not listed above.

---

### Task 6: `getpwnam_r` result through `found`

**Files:** Modify `adapter_socket/src/agent_user_policy.cpp`

- [ ] **Step 1:** In `resolveUserId`, return `static_cast<uint32_t>(found->pw_uid);` instead of `entry.pw_uid`.
  Do **not** turn `found` into a smart pointer: it points at the stack variable `entry` and owns nothing.
- [ ] **Step 2: Run** `make test-one-unit PATTERN=AgentUserPolicy` after `make rebuild-unit`. Expected: PASS.

---

### Task 7: Docs that describe the changed wiring

**Files:**
- Modify: `docs/socket-deadline.md` — the bullet "Deadlines come from a factory": the factory is now injected into
  `SocketEndpoint` (accept deadline) and passed to `FramedConnection` (one deadline per frame);
  `UnixSocketListenerFactory`/`UnixSocketListener`/`UnixSocketConnection` receive a `const IDeadline&` per call.
  Change **Status** to "implemented 2026-10-24".
- Modify: `docs/socket-endpoint.md` — wherever it shows the `SocketEndpoint` constructor, `acceptOne()` or
  `ISocketConnection::peerCredentials()`, update to the new signatures and `AcceptedConnectionDto`. Read the file
  first; change only those statements.
- Modify: `CLAUDE.md` (repo root of this repo), section Dependencies: `nlohmann_json/3.11.3` line → "(linked PUBLIC
  by `adapter_socket`; used by the parser next)" — `Readiness` no longer uses it.

- [ ] **Step 1:** Make the three edits. Keep the surrounding style (short bullets, backticked identifiers).

---

### Task 8: Full verification

- [ ] `make format` then `make format-check` → no diff reported.
- [ ] `make tidy-check` → no warnings. Fix any (typical: condition with a call inside `if`, missing `[[nodiscard]]`,
  magic number, unused include).
- [ ] `make rebuild && make rebuild-unit && make test-unit` → all PASS.
- [ ] `make rebuild-asan && make test-asan` → all PASS.
- [ ] `make rebuild-valgrind && make test-valgrind` → all PASS, no leaks.
- [ ] `grep -rn "configureTimeouts\|operationName\|peerCredentials()\|DEFAULT_ACCEPT_TIMEOUT\|UnixSocketConnection::DEFAULT_TIMEOUT\|nlohmann" adapter_socket`
  → only `nlohmann_json` in `adapter_socket/CMakeLists.txt`.
- [ ] Report: per task, done / deviated (and why), plus the final output lines of each `make test-*`.
