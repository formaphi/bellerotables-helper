# Socket roles: the helper is the server

Who is the server and who is the client on the agent ↔ helper socket, and why the helper does not look
like a server even though it is one. Transport details are in
[socket-stream-and-framing.md](socket-stream-and-framing.md); the decision itself is hub
`docs/decisions/helper-socket-posix.md`.

## Roles

On a socket, the **server** is the side that waits for connections (`listen` → `accept`) and the
**client** is the side that opens one (`connect`).

| | `bellerotables-helper` | `bellero-agent` |
|---|---|---|
| Socket role | **server** | **client** |
| Socket call | `accept()` on the listening fd systemd passes in | `connect()` to `/run/bellerophon/bellerotables-helper.sock` |
| Initiative | none: it only reacts to the connection it accepts | all of it: it opens the connection and sends the frame |
| Lifetime | one connection per process, then exit | long-running JVM, connects once per request |
| Phase 1 | reads one frame, applies, closes | sends one frame (`{correlation_id, id, desired}`) |
| Phase 2 | also writes one reply frame before closing | also reads the reply frame |

## Who listens: systemd

The helper never calls `socket()`, `bind()` or `listen()`. systemd does, from the `.socket` unit
(`ListenStream=/run/bellerophon/bellerotables-helper.sock`, `Accept=no`, `SocketMode=0660`). When a
client connects, systemd starts the helper and hands it the listening fd (fd 3, read with
`sd_listen_fds()`). The helper `accept()`s exactly one connection, serves it and exits. Further pending
connections stay in the kernel backlog for the next activation.

```
bellero-agent (client)           systemd                          bellerotables-helper (server)
        |                           | bind + listen on the path              |
        |------ connect() --------->| start the service, pass fd 3 --------->|
        |                           |                                        | sd_listen_fds → fd 3
        |<===================== accepted connection =======================>| accept() one
        |------ frame (desired) ------------------------------------------->| SO_PEERCRED check
        |                           |                                        | read frame, apply
        |<----- reply frame (phase 2) --------------------------------------|
        |                           |                                        | close, exit
```

## Why it does not look like a server

- **It is one-shot.** It serves a single connection and exits; it is not a daemon. The process that stays
  up is systemd, holding the socket.
- **It is called on demand.** From the agent's side the exchange works like a remote function call:
  connect, send, (receive), and the helper process is gone.
- **It never starts anything.** Every exchange starts on the agent's side.

So the helper is a **short-lived, socket-activated server**: a server by socket role, a throwaway tool by
lifecycle.

## Server-side flow

One activation runs these steps in order: **activation → listen → peer policy → connection → framing**.

1. **Activation.** Take fd 3 with `sd_listen_fds()` and check it is exactly one listening `AF_UNIX` stream
   socket. Fails with `ActivationException` (wrong fd count or kind) or `std::system_error` (libsystemd
   call failed).
2. **Listen.** One `accept()`, bounded by a deadline. Fails with `std::system_error` (`ETIMEDOUT` when the
   deadline passes, the `accept4` errno otherwise).
3. **Peer policy.** `AgentUserPolicy` compares the connection's `SO_PEERCRED` uid with the `agent_user` from
   `/etc/bellerophon/bellerotables-helper.conf`. It does not throw: a missing file or an unknown user makes
   it refuse every peer. A peer whose uid is `PeerCredentials::UNKNOWN_ID` (0, the value of credentials the
   kernel never filled, and also root) is always refused, even if `agent_user` resolves to uid 0, so
   `agent_user=root` never authorizes anyone.
4. **Connection.** Byte I/O on the accepted connection, bounded by a deadline. Fails with
   `ConnectionClosedException` (the peer closed mid-read) or `std::system_error` (timeout or syscall errno).
5. **Framing.** 4-byte big-endian length prefix and a 64 KiB payload cap. Fails with
   `FrameTooLargeException` (the declared or written length is above the cap, checked before the payload is
   read) or `ConnectionClosedException` (the stream ended inside the header).

Exception types: `ActivationException`, `ConnectionClosedException`, `FrameTooLargeException`
(`include/adapter_socket/exceptions/socket_exception.h`), plus `std::system_error` carrying the errno.
Mapping failures to a reply is the controller's job (phase 2).

## What this means for `adapter_socket`

Because the helper is the server, the library carries the server-side pieces a client would not need:

| Piece | Server concern |
|---|---|
| `SystemdActivation` | takes the listening fd from systemd and checks it is a listening `AF_UNIX` stream socket |
| `UnixSocketListener` (`ISocketListener`) | the single `accept()`, bounded by a deadline |
| `AgentUserPolicy` (`IPeerPolicy`) | refuses any peer whose `SO_PEERCRED` uid is not the configured `agent_user`, and always refuses uid `UNKNOWN_ID` (0) |
| `UnixSocketConnection` (`ISocketConnection`) | byte I/O on the accepted connection, bounded by a deadline |
| `FramedConnection` (`IFramedConnection`) | the 4-byte length prefix and the 64 KiB cap |

The agent's side (`connect()`, writing the frame) lives in `bellero-agent`, in Java.

These pieces are chained behind a single entry, `ISocketEndpoint::acceptAuthorized()`, so the controller
only sees `IFramedConnection`: [socket-endpoint.md](socket-endpoint.md).

## Alternative not taken: `Accept=yes`

**Decision: stay on `Accept=no`** (hub `docs/decisions/helper-socket-posix.md`). The comparison below
records why; reopening it is a hub decision, not a change in this repo.

### How it would work

With `Accept=yes`, systemd itself calls `accept()`. For every connection it starts a new instance of a
template unit, `bellerotables-helper@.service`, and passes the **already-connected** fd instead of the
listening one.

### What it would simplify

- `UnixSocketListener` and `ISocketListener` disappear, together with the accept deadline and the
  `EAGAIN` / `ECONNABORTED` retry loop.
- `SystemdActivation` gets simpler: it checks a connected socket instead of a listening one. With the
  inetd-style variant (`StandardInput=socket`) the connection arrives as stdin/stdout and libsystemd is
  not needed at all.
- Edge cases go away, such as the helper starting after the client already gave up.
- Unchanged: the `SO_PEERCRED` check, framing, read/write deadlines, and one process per request (which
  `Accept=no` also has).

### What it would make worse

- **Concurrency — the deciding point.** With `Accept=no`, requests are **serialized for free**: while the
  helper runs, systemd does not start another one, and new connections wait in the kernel backlog. With
  `Accept=yes`, simultaneous connections become **parallel helper instances** (up to `MaxConnections=`,
  default 64).
- **That race is dangerous for iptables.** libiptc reads the whole table, changes it in memory and commits
  the whole table back. Two helpers in parallel give "last writer wins": one rule is silently lost. The
  `xtables.lock` is taken by the `iptables` CLI (`-w`); the helper's direct libiptc calls are not assumed to
  take it (not verified in the libiptc source).
- **`MaxConnections=1` does not fix it.** When the limit is reached, systemd **refuses** extra connections
  instead of queueing them, so the agent would get a connection error and need retry logic.
- Smaller costs: a template unit (`@.service`), one named instance per connection (more journal noise), and
  failure handling per instance instead of per service.

### Side by side

| | `Accept=no` (current) | `Accept=yes` |
|---|---|---|
| Who calls `accept()` | the helper | systemd |
| fd passed to the helper | listening socket | connected socket |
| Listener in the code | yes | no |
| Simultaneous requests | serialized, waiting in the backlog | parallel instances, or refused with `MaxConnections=1` |
| iptables race | none | real; would need a lock of its own |
| Unit | plain `.service` | `@.service` template |

The one class `Accept=yes` would save does not pay for a race on the kernel mutation that serializing one
connection per activation currently prevents.
