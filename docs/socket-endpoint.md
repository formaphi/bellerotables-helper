# Socket endpoint: one entry, one exit for `adapter_socket`

**Status:** accepted 2026-10-23 — implemented 2026-10-23 (`socket_endpoint.h`,
`unix_socket_listener_factory.h`; tests `socket_endpoint_test.cpp`, `unix_socket_listener_factory_test.cpp`).
**Scope:** `adapter_socket` public surface and how the controller (Ring 3) consumes it.
**Background:** [socket-roles.md](socket-roles.md) (the helper is the server, the server-side flow),
[socket-stream-and-framing.md](socket-stream-and-framing.md) (wire format).

## Context

One activation runs five steps in order: **activation → listen → peer policy → connection → framing**.
Today the library exposes each step as a separate public piece (`SystemdActivation`, `UnixSocketListener`,
`AgentUserPolicy`, `UnixSocketConnection`, `FramedConnection`). Whoever uses the library has to chain them
in the right order, so the flow leaks out of the library and a caller can get it wrong — for example by
wrapping a connection in `FramedConnection` without checking the peer.

Seen from the flow, the library has one natural **entry** (the fd systemd passes in) and one natural
**exit** (frames, through `IFramedConnection`). Listener, peer policy and connection are internal steps the
controller should not need to know.

## Decision

1. **`ISocketEndpoint` is the library's single entry.** One method, `acceptAuthorized()`, runs activation,
   accept and the peer check, and returns the exit, an `IFramedConnection`.
2. **A refused peer is an empty `std::optional`**, not an exception:
   `std::optional<std::unique_ptr<IFramedConnection>>`. A non-empty optional never holds a `nullptr`.
3. **Activation is injected into the endpoint** through a new interface, `ISocketActivation`, implemented
   by `SystemdActivation` (its `takeListeningFd()` stops being `static`).
4. **The endpoint builds its listener through a factory**, `ISocketListenerFactory`, implemented by
   `UnixSocketListenerFactory`, because the listener needs the activated fd in its constructor.
5. **The endpoint is not injected into `FramedConnection`.** Dependencies point one way only: each layer
   receives the layer below it (see "Rejected alternatives").

## Interfaces

```cpp
class ISocketEndpoint
{
public:
    virtual ~ISocketEndpoint() = default;

    virtual std::optional<std::unique_ptr<IFramedConnection>> acceptAuthorized() = 0;
};

class ISocketActivation
{
public:
    virtual ~ISocketActivation() = default;

    [[nodiscard]] virtual std::unique_ptr<IFileDescriptor> takeListeningFd() const = 0;
};

class ISocketListenerFactory
{
public:
    virtual ~ISocketListenerFactory() = default;

    [[nodiscard]] virtual std::unique_ptr<ISocketListener>
    create(std::unique_ptr<IFileDescriptor> listeningFd) const = 0;
};
```

Files, per `cpp-interface` / `cpp-concrete-class`:

| Interface | Header | Concrete class | Mock |
|---|---|---|---|
| `ISocketEndpoint` | `interface_socket_endpoint.h` | `SocketEndpoint` (`socket_endpoint.h`) | `mock/mock_socket_endpoint.h` |
| `ISocketActivation` | `interface_socket_activation.h` | `SystemdActivation` (`systemd_activation.h`) | `mock/mock_socket_activation.h` |
| `ISocketListenerFactory` | `interface_socket_listener_factory.h` | `UnixSocketListenerFactory` (`unix_socket_listener_factory.h`) | `mock/mock_socket_listener_factory.h` |

All headers under `include/adapter_socket/`; the existing `IFramedConnection`, `ISocketListener`,
`IPeerPolicy` and `ISocketConnection` stay as they are.

## Flows

### Dependency graph (who receives what)

```
RequestController         ← ISocketEndpoint                                   (controller receives the entry)
SocketEndpoint            ← ISocketActivation + ISocketListenerFactory + IPeerPolicy
UnixSocketListenerFactory → creates UnixSocketListener (ISocketListener)
UnixSocketListener        → creates UnixSocketConnection (ISocketConnection) on accept
SocketEndpoint            → creates FramedConnection (IFramedConnection) after the peer check
FramedConnection          ← ISocketConnection                                 (adapter receives the adaptee)
```

The controller depends on `ISocketEndpoint` and `IFramedConnection` only.

### Construction (once per process)

```
composition root
  └─ SocketEndpoint(activation, listenerFactory, peerPolicy)
       ├─ activation->takeListeningFd()      sd_listen_fds → fd 3, validated
       └─ listenerFactory->create(fd)        UnixSocketListener owns fd 3
```

Activation runs in the endpoint's constructor, exactly once: `sd_listen_fds()` unsets the activation
environment, so a second call would fail. A failed activation throws from the constructor
(`ActivationException` / `std::system_error`) and the process exits before any connection exists.

### `acceptAuthorized()` (once per activation)

```
acceptAuthorized()
  ├─ deadline = m_deadlineFactory->startingNow(acceptTimeout)
  ├─ accepted = m_listener->acceptOne(deadline)     accept() bounded by the deadline; SO_PEERCRED read on accept
  │                                                 → AcceptedConnectionDto { connection, peer }
  ├─ m_peerPolicy->isAllowed(accepted.peer)
  │     ├─ false → return std::nullopt             connection is released → fd closed → agent sees EOF
  │     └─ true  ↓
  └─ return std::make_unique<FramedConnection>(std::move(accepted.connection), m_deadlineFactory, connectionTimeout)
```

Failures that are not a refused peer still throw: accept timeout or syscall error (`std::system_error`).

### End to end

```
bellero-agent        systemd           SocketEndpoint                 RequestController
     | connect() ------>| start, fd 3 ----->| ctor: activate + create listener
     |                  |                   |<------------ acceptAuthorized() ---|
     |<========= accepted connection ======>| peer check                         |
     |                  |                   |---- optional<IFramedConnection> -->|
     |------------------------- frame (desired) -------------------------------->| readFrame()
     |<------------------------ reply frame (phase 2) ---------------------------| writeFrame()
     |                  |                   |                                    | exit
```

## Examples

Examples follow `cpp-style`: no call inside an argument list or a condition, sinks by value + `std::move`,
non-stored `shared_ptr` parameters by `const&`.

### Endpoint (inside the library)

```cpp
class SocketEndpoint final : public ISocketEndpoint
{
public:
    SocketEndpoint(const std::shared_ptr<ISocketActivation>& activation,
                   const std::shared_ptr<ISocketListenerFactory>& listenerFactory,
                   std::unique_ptr<IPeerPolicy> peerPolicy,
                   std::shared_ptr<IDeadlineFactory> deadlineFactory,
                   const SocketTimeoutsDto& timeouts = SocketTimeoutsDto());
    ~SocketEndpoint() override = default;

    SocketEndpoint(const SocketEndpoint&) = delete;
    SocketEndpoint& operator=(const SocketEndpoint&) = delete;
    SocketEndpoint(SocketEndpoint&&) = delete;
    SocketEndpoint& operator=(SocketEndpoint&&) = delete;

    std::optional<std::unique_ptr<IFramedConnection>> acceptAuthorized() override;

private:
    static std::unique_ptr<ISocketListener>
    createListener(const std::shared_ptr<ISocketActivation>& activation,
                   const std::shared_ptr<ISocketListenerFactory>& listenerFactory);

    std::unique_ptr<ISocketListener> m_listener;
    std::unique_ptr<IPeerPolicy> m_peerPolicy;
    std::shared_ptr<IDeadlineFactory> m_deadlineFactory;
    SocketTimeoutsDto m_timeouts;
};
```

```cpp
SocketEndpoint::SocketEndpoint(const std::shared_ptr<ISocketActivation>& activation,
                               const std::shared_ptr<ISocketListenerFactory>& listenerFactory,
                               std::unique_ptr<IPeerPolicy> peerPolicy,
                               std::shared_ptr<IDeadlineFactory> deadlineFactory,
                               const SocketTimeoutsDto& timeouts)
    : m_listener(createListener(activation, listenerFactory))
    , m_peerPolicy(std::move(peerPolicy))
    , m_deadlineFactory(std::move(deadlineFactory))
    , m_timeouts(timeouts)
{
}

std::unique_ptr<ISocketListener>
SocketEndpoint::createListener(const std::shared_ptr<ISocketActivation>& activation,
                               const std::shared_ptr<ISocketListenerFactory>& listenerFactory)
{
    std::unique_ptr<IFileDescriptor> listeningFd = activation->takeListeningFd();
    return listenerFactory->create(std::move(listeningFd));
}

std::optional<std::unique_ptr<IFramedConnection>> SocketEndpoint::acceptAuthorized()
{
    const std::unique_ptr<IDeadline> acceptDeadline =
        m_deadlineFactory->startingNow(m_timeouts.acceptTimeout);
    AcceptedConnectionDto accepted = m_listener->acceptOne(*acceptDeadline);
    const bool isAuthorized = m_peerPolicy->isAllowed(accepted.peer);
    if (!isAuthorized)
    {
        return std::nullopt;
    }
    return std::make_unique<FramedConnection>(std::move(accepted.connection), m_deadlineFactory,
                                              m_timeouts.connectionTimeout);
}
```

### Listener factory

```cpp
class UnixSocketListenerFactory final : public ISocketListenerFactory
{
public:
    explicit UnixSocketListenerFactory(
        std::shared_ptr<IDeadlineFactory> deadlineFactory,
        std::chrono::milliseconds acceptTimeout = UnixSocketListener::DEFAULT_ACCEPT_TIMEOUT,
        std::chrono::milliseconds connectionTimeout = UnixSocketConnection::DEFAULT_TIMEOUT);
    ~UnixSocketListenerFactory() override = default;

    UnixSocketListenerFactory(const UnixSocketListenerFactory&) = delete;
    UnixSocketListenerFactory& operator=(const UnixSocketListenerFactory&) = delete;
    UnixSocketListenerFactory(UnixSocketListenerFactory&&) = delete;
    UnixSocketListenerFactory& operator=(UnixSocketListenerFactory&&) = delete;

    [[nodiscard]] std::unique_ptr<ISocketListener>
    create(std::unique_ptr<IFileDescriptor> listeningFd) const override;

private:
    std::shared_ptr<IDeadlineFactory> m_deadlineFactory;
    std::chrono::milliseconds m_acceptTimeout;
    std::chrono::milliseconds m_connectionTimeout;
};

UnixSocketListenerFactory::UnixSocketListenerFactory(
    std::shared_ptr<IDeadlineFactory> deadlineFactory, std::chrono::milliseconds acceptTimeout,
    std::chrono::milliseconds connectionTimeout)
    : m_deadlineFactory(std::move(deadlineFactory))
    , m_acceptTimeout(acceptTimeout)
    , m_connectionTimeout(connectionTimeout)
{
    const bool hasInvalidTimeout = acceptTimeout <= NO_TIMEOUT || connectionTimeout <= NO_TIMEOUT;
    if (hasInvalidTimeout)
    {
        throw std::invalid_argument("accept and connection timeouts must be greater than zero");
    }
}

std::unique_ptr<ISocketListener>
UnixSocketListenerFactory::create(std::unique_ptr<IFileDescriptor> listeningFd) const
{
    return std::make_unique<UnixSocketListener>(std::move(listeningFd), m_deadlineFactory,
                                                m_acceptTimeout, m_connectionTimeout);
}
```

The factory validates the timeouts in its constructor, so a bad configuration fails in the composition root
before activation takes fd 3, not inside `create()`. It also hands the `IDeadlineFactory` to every listener,
which hands it to every connection ([socket-deadline.md](socket-deadline.md)).

### Composition root (the future `main`)

The only place that names concrete classes:

```cpp
const std::shared_ptr<ISocketActivation> activation = std::make_shared<SystemdActivation>();
const std::shared_ptr<IDeadlineFactory> deadlineFactory = std::make_shared<SteadyDeadlineFactory>();
const std::shared_ptr<ISocketListenerFactory> listenerFactory =
    std::make_shared<UnixSocketListenerFactory>();
std::unique_ptr<IPeerPolicy> peerPolicy = std::make_unique<AgentUserPolicy>();
const std::shared_ptr<ISocketEndpoint> endpoint = std::make_shared<SocketEndpoint>(
    activation, listenerFactory, std::move(peerPolicy), deadlineFactory, SocketTimeoutsDto::create(acceptTimeout, connectionTimeout));
const std::shared_ptr<IRequestController> controller =
    std::make_shared<RequestController>(endpoint, parser);
controller->serveOne();
```

### Consumer (the controller, Ring 3)

```cpp
void RequestController::serveOne()
{
    const std::optional<std::unique_ptr<IFramedConnection>> connection = m_endpoint->acceptAuthorized();
    if (!connection.has_value())
    {
        return;
    }

    const std::vector<uint8_t> payload = connection.value()->readFrame();
    // parser → use case; phase 2: connection.value()->writeFrame(reply)
}
```

What the controller does with a refused peer beyond returning (log, exit code) is decided with the
controller.

### Tests

The endpoint, with no socket and no systemd:

```cpp
TEST_F(SocketEndpointTest, ReturnsEmptyWhenPeerIsRefused)
{
    expectActivatedFdReachesFactory();
    EXPECT_CALL(*m_listener, acceptOne(_))
        .WillOnce(Return(ByMove(acceptedFrom(STRANGER_PEER))));
    EXPECT_CALL(*m_peerPolicy, isAllowed(_)).WillOnce(Return(false));

    SocketEndpoint endpoint(m_activation, m_listenerFactory, std::move(m_ownedPeerPolicy),
                            m_deadlineFactory);

    EXPECT_FALSE(endpoint.acceptAuthorized().has_value());
}
```

The controller, with no socket, no systemd and no frame header bytes:

```cpp
EXPECT_CALL(*m_endpoint, acceptAuthorized())
    .WillOnce(Return(ByMove(std::make_unique<MockFramedConnection>())));
EXPECT_CALL(*m_framedConnection, readFrame()).WillOnce(Return(SAMPLE_DESIRED_PAYLOAD));
```

`expectActivatedFdReachesFactory()` makes the activation return a `MockFileDescriptor` (`ByMove`) and
expects the factory to receive that same object.

Mocks used: `MockFileDescriptor`, `MockSocketActivation`, `MockSocketListenerFactory`, `MockSocketListener`,
`MockSocketConnection`, `MockPeerPolicy` (endpoint); `MockSocketEndpoint`, `MockFramedConnection`
(controller).

## Rejected alternatives

- **Inject the endpoint into `FramedConnection`.** Rejected:
  - *Circular dependency* — the endpoint returns `IFramedConnection`; the framed connection depending on the
    endpoint closes a loop.
  - *Different lifetimes* — the endpoint is the process's door and produces connections; a framed connection
    is one accepted connection. Accepting inside `FramedConnection` would hide an `accept()` in the first
    `readFrame()`.
  - *SRP and tests* — framing is tested today with a fake `ISocketConnection` that delivers bytes in chunks;
    with the endpoint inside, every framing test would also have to fake accept and the peer check.
- **Throw on a refused peer** (`PeerRefusedException`). Rejected in favour of an empty optional: a refused
  peer is an expected outcome of the check, and the caller handles it without a `try`.
- **Activation in the composition root** (the root calls `takeListeningFd()` and builds the listener).
  Rejected: the activation → listener order would leak into the root; with `ISocketActivation` and the
  factory injected, the whole flow stays inside the library and the endpoint is unit-testable.

## Consequences

- The controller depends on two library types, `ISocketEndpoint` and `IFramedConnection`; the internal
  steps can change without touching it. The `FileDescriptor` change
  ([socket-file-descriptor.md](socket-file-descriptor.md)) already did: `ISocketActivation::takeListeningFd()` and
  `ISocketListenerFactory::create()` carry a `std::unique_ptr<IFileDescriptor>` instead of an `int`, so fd 3 is
  owned from the moment activation validates it.
- The peer check can no longer be skipped: there is no public path from an accepted connection to a framed
  one that bypasses it.
- `SystemdActivation::takeListeningFd()` becomes a `const` override of `ISocketActivation` instead of a
  `static` function.
- With the peer validated inside the endpoint, `peerCredentials()` left `IFramedConnection`, which now has
  `readFrame()` and `writeFrame()` only (ISP; `socket-review.md`, Versão 2, item 2).
