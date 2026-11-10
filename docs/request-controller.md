# Request controller: one activation, one outcome

**Status:** proposed 2026-11-04, not implemented.
**Scope:** the controller (Ring 3, backlog H-204), the `domain/` library it hands requests to, and the
`adapter_logger/` library it reports through. Phase 1 only (no reply frame); the phase-2 slot is marked.
**Background:** [socket-endpoint.md](socket-endpoint.md) (the controller's entry, `ISocketEndpoint`),
[syscall-adapters.md](syscall-adapters.md) (`SystemAdapters::createPosix()`), `task-request-parser.md` (the
parser it calls), [plan/backlog.md](plan/backlog.md) (E2), hub `docs/decisions/agent-two-phase-delivery.md`.

## Context

`adapter_socket` gives the helper one entry, `ISocketEndpoint::acceptAuthorized()`, which returns a framed
connection or nothing for a refused peer. The parser (sprint 01) will turn one frame's payload into a request or
a typed `ParseProblem`. Nothing connects the two to the code that changes the kernel, and `main.cpp` is an empty
`main`.

Phase 1 has no reply frame (hub `agent-two-phase-delivery.md`), so the log line and the exit code are the only
trace a request leaves. Whatever ends an activation, the operator must be able to see why with `systemctl status`
and `journalctl`.

Two facts from the current code shape the design:

- `SocketEndpoint`'s constructor already calls `takeListeningFd()`. A missing or wrong activated fd throws
  (`ActivationException`, `std::system_error`) while `main.cpp` builds the object graph, before the controller
  runs.
- A refused peer comes back as an empty `std::optional` with no uid, so the controller cannot log who was
  refused. Adding it would change `adapter_socket` and is out of scope.

## Decision

1. **The controller owns the whole activation.** `RequestController::serveOne()` runs accept → read → parse →
   execute and returns a `ServeOutcome`. `main.cpp` only composes objects, catches startup failures and returns
   `serveOne().exitCode()`.
2. **A `domain/` library (Ring 2) holds the request and the use-case contract**: `RedirectionRequest`,
   `DesiredRedirection`, `Logging`, `IRedirectionUseCase`, `RedirectionProblem`. It depends on nothing. The
   parser maps into it; the use case implements it. Protobuf stays inside the parser library.
3. **The use case is `IRedirectionUseCase::execute()`**, returning `std::expected<void, RedirectionProblem>`.
   Expected failures are values. E3 changes the success type to the observed kernel state once readback exists,
   so `reported` is shaped by the kernel and not guessed now.
4. **Logging goes through `ILogger`, backed by spdlog**, in a new `adapter_logger/` library (Ring 4), the only
   library that links spdlog. `main.cpp` picks the sink: spdlog's `systemd_sink` in production, its stderr sink in
   the sandbox.
5. **One log line per activation**, written at the step that ended it, with `correlation_id` and `id` once known.
   The raw payload is never logged.
6. **Every outcome other than success exits non-zero**, with a `sysexits.h` code that names where to look.

## Components

```
                 main.cpp  (composition root only)
                    │ builds everything, calls serveOne(), returns the exit code
                    ▼
 controller/  (Ring 3)    RequestController ── ServeOutcome
     │            │             │                  │
     ▼            ▼             ▼                  ▼
 adapter_socket  parser       domain/            adapter_logger/
 (Ring 4)        (Ring 3)     (Ring 2)           (Ring 4)
 ISocketEndpoint IRequestParser  RedirectionRequest   ILogger
 IFramedConn.        │       Logging             SpdlogLogger ── spdlog
                     └──────▶ IRedirectionUseCase
                              RedirectionProblem
```

| Library | Holds | Depends on | Links |
|---|---|---|---|
| `domain/` | `RedirectionRequest`, `DesiredRedirection`, `Logging`, `IRedirectionUseCase`, `RedirectionProblem`, `MockRedirectionUseCase` | nothing | nothing |
| `adapter_logger/` | `ILogger`, `SpdlogLogger`, `MockLogger` | nothing in the repo | spdlog (with fmt) |
| `controller/` | `IRequestController`, `RequestController`, `ServeOutcome` | `adapter_socket` interfaces, `IRequestParser`, `domain/`, `ILogger` | no protobuf, no spdlog, no libiptc |
| parser (sprint 01) | `IRequestParser`, the concrete parser, `ParseProblem`, `MockRequestParser` | `domain/` | protobuf |

The parser library's name is chosen in sprint 01 (T-102.1); it is written `request_parser` below.

## Interfaces

Comments follow the repo's modifier rule.

### `domain/`

```cpp
namespace domain {

// LEARN: mirrors bellerophon.ngfw.shared.Logging; presence of the optional fields is kept.
struct Logging
{
    int32_t group;
    std::optional<std::string> prefix;
    std::optional<int32_t> range;
    std::optional<int32_t> threshold;
};

// LEARN: logging absent means "disabled" (presence-as-toggle in DesiredRedirection).
struct DesiredRedirection
{
    std::optional<Logging> logging;
};

struct RedirectionRequest
{
    std::string correlationId;
    int32_t id;
    DesiredRedirection desired;
};

enum class RedirectionProblemKind
{
    KernelRejected,
    TableUnavailable,
    Unexpected
};

struct RedirectionProblem
{
    RedirectionProblemKind kind;
    std::string detail;
};

class IRedirectionUseCase
{
public:
    virtual ~IRedirectionUseCase() = default;
    virtual std::expected<void, RedirectionProblem> execute(const RedirectionRequest& request) = 0;
};

} // namespace domain
```

`RedirectionProblemKind` holds the kinds the controller needs to log today; the use case (H-205) may add kinds.
`Unexpected` is set only by the controller's defensive catch (see Flows).

### Contract sources (`contracts` at `e86bc8f`)

| Domain element | Contract source | Note |
|---|---|---|
| `Logging` | `bellerophon/ngfw/shared/logging.proto`, `Logging` | same fields, types and presence; the protovalidate rules are not repeated here (see the parser's protovalidate decision) |
| `DesiredRedirection` | `bellerophon/ngfw/redirection/desired.proto`, `DesiredRedirection` | `logging` absent means disabled |
| `RedirectionRequest::correlationId` | `bellerophon/ngfw/common/problem.proto`, `Accepted.correlation_id` | UUID string |
| `RedirectionRequest::id` | `bellerophon/ngfw/common/problem.proto`, `Accepted.id` | `int32` |
| the envelope `{correlation_id, id, desired}` | none | ad-hoc JSON until a contract message exists (`task-request-parser.md`, hub `status.md`) |
| `RedirectionProblem` | none, helper-internal | phase 2 maps it to `ProblemDetails` (`common/problem.proto`) on the wire |
| E3's observed state | `bellerophon/ngfw/redirection/reported.proto`, `ReportedRedirection` | today mirrors `Logging`; kernel-only fields come later |

The domain types are hand-written mirrors: `domain/` neither includes nor links anything from `contracts`. The
build dependency on `contracts` runs only through the parser. A field renamed or retyped in these files breaks
the parser's mapping at compile time. A field **added** to them does not: protobuf accepts
it and the mapping ignores it, so the helper would drop it silently. The parser's contract drift test (sprint 01,
T-102.4) compares each generated message's descriptor with the fields the mapping handles and fails on an
unmapped field. The domain types are then updated to match.

### `adapter_logger/`

```cpp
namespace adapter_logger {

class ILogger
{
public:
    virtual ~ILogger() = default;
    virtual void info(std::string_view message) = 0;
    virtual void warning(std::string_view message) = 0;
    virtual void error(std::string_view message) = 0;
};

class SpdlogLogger final : public ILogger
{
public:
    explicit SpdlogLogger(spdlog::sink_ptr sink);
    ~SpdlogLogger() override = default;
    SpdlogLogger(const SpdlogLogger&) = delete;
    SpdlogLogger& operator=(const SpdlogLogger&) = delete;
    SpdlogLogger(SpdlogLogger&&) = delete;
    SpdlogLogger& operator=(SpdlogLogger&&) = delete;
    void info(std::string_view message) override;
    void warning(std::string_view message) override;
    void error(std::string_view message) override;

private:
    std::unique_ptr<spdlog::logger> m_logger;
};

} // namespace adapter_logger
```

`spdlog::sink_ptr` is spdlog's own `std::shared_ptr` type; it stays inside `adapter_logger` and `main.cpp`.

### `controller/`

```cpp
namespace controller {

enum class ServeOutcomeKind
{
    Succeeded,
    PeerRefused,
    TransportFailed,
    Rejected,
    ExecutionFailed
};

struct ServeOutcome
{
    ServeOutcomeKind kind;
    [[nodiscard]] int exitCode() const;
};

class IRequestController
{
public:
    virtual ~IRequestController() = default;
    virtual ServeOutcome serveOne() = 0;
};

class RequestController final : public IRequestController
{
public:
    RequestController(std::unique_ptr<adapter_socket::ISocketEndpoint> endpoint,
                      std::unique_ptr<request_parser::IRequestParser> parser,
                      std::unique_ptr<domain::IRedirectionUseCase> useCase,
                      adapter_logger::ILogger& logger);
    ~RequestController() override = default;
    RequestController(const RequestController&) = delete;
    RequestController& operator=(const RequestController&) = delete;
    RequestController(RequestController&&) = delete;
    RequestController& operator=(RequestController&&) = delete;
    ServeOutcome serveOne() override;

private:
    std::unique_ptr<adapter_socket::ISocketEndpoint> m_endpoint;
    std::unique_ptr<request_parser::IRequestParser> m_parser;
    std::unique_ptr<domain::IRedirectionUseCase> m_useCase;
    adapter_logger::ILogger& m_logger;
};

} // namespace controller
```

The logger is a reference because `main.cpp` owns it and needs it before the controller exists, to log startup
failures.

## Flows

### `serveOne()` (once per activation)

```
RequestController::serveOne() -> ServeOutcome
│
├─ 1. endpoint->acceptAuthorized()
│     empty optional ................................ PeerRefused     warning "peer refused"
│     std::system_error (accept timeout, accept4, getsockopt, poll)
│                    ................................ TransportFailed error   "accept failed: <what>"
│
├─ 2. connection->readFrame()
│     ConnectionClosedException ..................... TransportFailed warning "peer closed before full frame"
│     FrameTooLargeException ........................ TransportFailed warning "frame above 64 KiB"
│     std::system_error (read deadline, recv, poll) . TransportFailed error   "read failed: <what>"
│
├─ 3. parser->parse(payload as std::string_view)
│     unexpected ParseProblem ....................... Rejected        warning "rejected kind=<kind> <detail>"
│
├─ 4. useCase->execute(request)
│     unexpected RedirectionProblem ................. ExecutionFailed error   "execution failed correlation_id=… id=… kind=<kind> <detail>"
│     any std::exception (defensive) ................ ExecutionFailed error   same, kind=Unexpected
│
├─ 5. phase 2 slot: connection->writeFrame(reply), before the connection is destroyed
│
└─ success .......................................... Succeeded       info   "succeeded correlation_id=… id=…"
```

- The connection is a local `std::unique_ptr`, so it closes when `serveOne()` returns, on every path.
- The controller catches only what each step documents: socket exceptions around steps 1 and 2; parser and
  use-case failures arrive as values.
- The catch of `std::exception` around step 4 is defensive. The use-case contract says expected failures are
  values and the implementation converts `adapter_iptc`'s exceptions; the catch only makes a use-case bug in a
  privileged process end as a logged `ExecutionFailed` with the request's ids, not an anonymous `terminate`.
- `readFrame()` returns `std::vector<uint8_t>`; the parser receives a `std::string_view` over the same bytes, no
  copy.
- No step runs after the one that ended the activation.

### Composition root (`main.cpp`)

```cpp
int main()
{
    const std::unique_ptr<adapter_logger::ILogger> logger = std::make_unique<adapter_logger::SpdlogLogger>(
        std::make_shared<spdlog::sinks::systemd_sink_st>());
    try
    {
        adapter_socket::SystemAdapters adapters = adapter_socket::SystemAdapters::createPosix();
        // LEARN: SocketEndpoint's constructor takes the activated fd; activation errors throw here.
        // TODO: build endpoint as in socket-endpoint.md "Composition root" (H-206)
        std::unique_ptr<adapter_socket::ISocketEndpoint> endpoint;
        // TODO: concrete parser from sprint 01 and the M0 use case from H-205 (H-206)
        std::unique_ptr<request_parser::IRequestParser> parser;
        std::unique_ptr<domain::IRedirectionUseCase> useCase;
        controller::RequestController requestController(std::move(endpoint), std::move(parser),
                                                        std::move(useCase), *logger);
        return requestController.serveOne().exitCode();
    }
    catch (const std::exception& startupFailure)
    {
        logger->error(std::string("startup failed: ") + startupFailure.what());
        return EX_CONFIG;
    }
}
```

The logger is built first so the startup path can log. The `TODO` lines are what H-206 (wiring) fills in; the
snippet shows the shape, not working code.

## Outcome → exit code

| Outcome | Exit code | Log priority | Where to look |
|---|---|---|---|
| `Succeeded` | 0 | info | |
| `Rejected` | 65 `EX_DATAERR` | warning | the agent, or a `contracts` pin mismatch |
| `PeerRefused` | 77 `EX_NOPERM` | warning | the socket group, or `agent_user` in the conf |
| `TransportFailed` | 74 `EX_IOERR` | warning or error | the agent's dispatcher |
| `ExecutionFailed` | 70 `EX_SOFTWARE` | error | the helper: libiptc, the kernel, or a use-case bug |
| startup failed (`main.cpp`) | 78 `EX_CONFIG` | error | the `.socket`/`.service` units |

systemd prints these by name (`status=65/DATAERR`). A non-zero exit marks that run `failed` in `systemctl` and
can trigger `OnFailure=`; it does not stop the next activation, and nothing is retried (no `Restart=`). The agent
decides whether to resend; the phase-2 reply will tell it why.

## Testing

**Unit** (`controller/test/`, gtest/gmock, no socket, no systemd, no kernel), with `MockSocketEndpoint` and
`MockFramedConnection` (existing), `MockRequestParser`, `MockRedirectionUseCase` and `MockLogger` (new):

- one test per flow path, parameterized where the shape repeats, asserting the `ServeOutcomeKind` and that
  nothing escapes `serveOne()`: refused peer; accept `system_error`; `ConnectionClosedException`,
  `FrameTooLargeException`, read `system_error`; parse problem; use-case problem; use case throwing; success;
- step isolation with `StrictMock`: no parse after a read failure, no `execute()` after a parse problem;
- exactly one log call per path, at the priority in the table, with `correlation_id` and `id` once known and
  never the payload;
- the parser receives the frame's bytes as text, including an empty and a 64 KiB payload;
- `ServeOutcome::exitCode()` against the table, one row per kind.

**`adapter_logger`:** `SpdlogLogger` on spdlog's `ostream_sink`; each method writes one line at its level.

**Integration** (`controller/test/`): a real `SocketEndpoint` on a listening Unix socket passed through
`LISTEN_FDS`/`LISTEN_PID` (as `systemd_activation_test.cpp` does), the real parser, a mock use case and a mock
logger. A client thread writes one valid frame; the test asserts `Succeeded` and the request `execute()` received.

**Gates:** unit, ASan and Valgrind green; `format-check` and `tidy-check` clean for `domain/`, `adapter_logger/`
and `controller/`. `main.cpp` has no test of its own; the `socat` driver (H-208) covers it end to end.

**Mutation checks** (revert each after): remove the catch around `readFrame()`; call `execute()` after a parse
problem; log twice on success; swap two exit codes. Each must fail a named test.

## Rejected alternatives

**The controller handles one accepted connection; `main.cpp` accepts.** Smaller controller, but the refused-peer
path and part of the exception mapping move into `main.cpp`, which has no unit test.

**The controller takes the payload bytes only.** Trivial to test, but socket failures and the phase-2
`writeFrame()` end up outside the controller.

**Domain types inside the parser library.** Fewer libraries, but the use case and everything that touches
`RedirectionRequest` would link the parser and with it protobuf, against the single blast radius of hub
`protobuf-consumer-facade.md`.

**One application library for controller, use-case interface and domain types.** The parser would depend on
the controller's library, and the ring diagram in `CLAUDE.md` would stop matching the code.

**The use case returns a `ReportedState` now.** Its fields would be guessed before readback and before the
`reported` envelope in `contracts`, which hub `agent-two-phase-delivery.md` says the helper should inform, not
guess.

**The use case throws on failure.** Inconsistent with the parser's `std::expected` and with "failures are
values" for expected errors.

**Exit code only, no logging.** Loses `correlation_id` and the parse detail, so a request stuck at `QUEUED`
cannot be traced.

**Own minimal logger, Boost.Log, glog or quill.** An own logger needs a second class for structured journal
fields; Boost.Log is heavy for a few lines per activation; glog has no journald sink and installs signal
handlers; quill's async backend thread needs a flush before a one-shot exit and would put TSan back in the
pre-commit gate.

**Exit 0 for `Rejected`.** Hides an agent/helper contract mismatch from `systemctl status`.

## Consequences

- **Sprint 01 changes before it starts:** T-102.2 creates `RedirectionRequest`, `DesiredRedirection` and
  `Logging` in `domain/`, not in the parser library; `IRequestParser` returns
  `std::expected<domain::RedirectionRequest, ParseProblem>`. `ParseProblem` stays in the parser.
- **New prerequisite work, independent of the parser:** `domain/` and `adapter_logger/` (spdlog in the Conan
  file, then `make sandbox-build`). The controller's unit tests use mocks and do not need `contracts`; only its
  integration test needs the real parser, so it waits on sprint 01 (H-101, H-102).
- **Contract drift:** sprint 01 gains the drift test (T-102.4) (see "Contract sources").
- **H-207 (systemd units) gains a requirement:** with `Accept=no`, each request is one service start, and the
  default start limit (5 starts in 10 s, whatever the exit code) would make systemd refuse the 6th request in
  10 s and put the socket in a failed state. The `.service` sets `StartLimitIntervalSec=0`, or a burst sized to the
  agent's rate, and H-207's test sends more than 5 requests in 10 s.
- **Yocto:** spdlog (meta-oe) joins protobuf in the helper recipe's dependencies (E5).
- **The phase-2 reply** adds step 5 and changes `IRedirectionUseCase`'s success type; both are E3.
- **The refused peer's uid** is not logged until `adapter_socket` returns it; a follow-up if operators need it.
