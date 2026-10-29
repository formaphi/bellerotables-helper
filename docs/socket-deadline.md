# Deadlines in `adapter_socket`

**Status:** implemented 2026-10-24.
**Origin:** the "Open" item of [socket-file-descriptor.md](socket-file-descriptor.md).

## Decision

- **`IDeadline` / `SteadyDeadline` own the time arithmetic.** `FileDescriptor::waitUntilReady` only asks the
  deadline and runs the `poll()`:

  ```cpp
  class IDeadline
  {
  public:
      virtual ~IDeadline() = default;

      [[nodiscard]] virtual bool hasExpired() const = 0;
      [[nodiscard]] virtual int pollTimeoutMilliseconds() const = 0;
  };
  ```

  `SteadyDeadline(timeout)` sets `expiresAt = steady_clock::now() + timeout` once, in its constructor.
  `pollTimeoutMilliseconds()` rounds the remaining time **up** (`ceil`) so `poll()` never wakes before the
  deadline, and caps it at `INT_MAX`.
- **Deadlines come from a factory.**

  ```cpp
  class IDeadlineFactory
  {
  public:
      virtual ~IDeadlineFactory() = default;

      [[nodiscard]] virtual std::unique_ptr<IDeadline>
      startingNow(std::chrono::milliseconds timeout) const = 0;
  };
  ```

  `SteadyDeadlineFactory` builds `SteadyDeadline`s. The caller that defines "one operation" opens the deadline:
  `SocketEndpoint` opens the accept deadline (`acceptTimeout`) and passes it to
  `ISocketListener::acceptOne(const IDeadline&)`; `FramedConnection` opens one deadline per frame
  (`connectionTimeout`) and passes it to `ISocketConnection::readExactly` / `writeAll`. A
  `std::shared_ptr<IDeadlineFactory>` is injected at the composition root into `SocketEndpoint` and
  `FramedConnection`. `UnixSocketListener`, `UnixSocketListenerFactory` and `UnixSocketConnection` hold only the fd.
- **`waitUntilReady(Readiness, const IDeadline&)`.** Each operation owns its deadline, and the fd only borrows it.
- **Tests control time.** A `MockDeadline` that has expired makes the timeout path immediate, with no real waits,
  in the fd, listener and connection tests. `SteadyDeadline` has its own tests for the rounding and the cap.

## Why a class and not a struct

The project uses a `struct` (DTO) for data that travels, with no behaviour beyond conversion and no rule to
protect. The deadline fails all three of those conditions:

- **Behaviour that reads the clock.** `hasExpired()` and `pollTimeoutMilliseconds()` read `steady_clock::now()` on
  every call. Their answer changes over time while no field does.
- **A rule to protect.** `expiresAt` must come from the monotonic clock and be fixed once, when the operation
  starts. If the field were public, a retry loop could push the deadline forward.
- **It must be mockable.** The testing gain depends on `IDeadline`, and `cpp-dto` forbids inheritance in DTOs.

A `struct DeadlineDto { time_point expiresAt; }` would just rename a `time_point`. The logic would stay in
`FileDescriptor`, which is the situation before this decision.

## Rejected

- **No factory: listener and connection build `SteadyDeadline(m_timeout)` themselves.** It is simpler, but the
  listener and connection tests would still need real time to reach a timeout.
- **An `IClock` instead of a deadline factory.** It is the same idea one level lower. Every caller would still
  repeat `now() + timeout` and the expiry check.

## Open

- None for the deadline itself.
