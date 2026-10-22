# Unix stream sockets and framing

Background for the helper's transport layer (`adapter_socket`). It explains why the code talks about
*frames*, how a Unix stream socket differs from UDP, and why the wire format is length-prefixed. The
transport decisions themselves live in the hub: `docs/decisions/helper-socket-posix.md`.

## Terminology

**Frame** comes from networking — the unit the link layer puts on the wire (an Ethernet frame). Application
protocols (HTTP/2, WebSocket, gRPC) reuse the word for **a message with a well-defined start and end inside
a stream of bytes**. **Framing** is the rule that says where one message ends and the next begins.

The vocabulary is only needed because some transports do not do this for you. A Unix stream socket is one
of them.

Throughout this repo: *stream* is the socket, *frame* is the message made of a 4-byte length prefix plus
payload.

## Datagram vs stream

| | UDP (`SOCK_DGRAM`) | Unix stream (`AF_UNIX`, `SOCK_STREAM`) |
|---|---|---|
| Unit | **datagram**: each `sendto` is one packet, each `recvfrom` returns one whole packet | **byte stream**: no messages, only a continuous sequence of bytes |
| Message boundary | exists, for free | **does not exist**; the application builds it (framing) |
| Connection | connectionless: `sendto` / `recvfrom` to any address | connected: `listen` → `accept`, then `read` / `write` on the same fd |
| Delivery | may be lost, duplicated, reordered | reliable and ordered (local, inside the kernel) |
| Address | IP + port | a **file path**, e.g. `/run/bellerophon/bellerotables-helper.sock` |
| Reach | over the network | same machine only, no IP stack |
| Who is the caller | source IP only, which can be forged | `SO_PEERCRED`: the kernel reports the peer's uid, gid and pid |
| Access control | firewall and port | **file permissions** on the socket path (`0660`, owner and group) |

Behaviourally a Unix stream socket is close to TCP, but local and without IP.

## Why framing is needed

With UDP, if the sender sends 10 bytes and then 20 bytes, the receiver gets two datagrams: 10 and 20. With
a stream the kernel only guarantees byte order, and the receiver may see:

```
sent:      [10 bytes][20 bytes]
received:  [30 bytes]        <- merged
     or:   [7][23]           <- cut in the middle
     or:   [10][5][15]       <- any partition
```

A `read(30)` may return fewer than 30 bytes (a **short read**) without any error. That is why the
connection adapter exposes `readExactly(n)`, which loops until it has collected *n* bytes, and why a frame
format is required: without it the receiver cannot tell how many bytes belong to one message.

## Ways to delimit a message in a stream

| Technique | How | Notes |
|-----------|-----|-------|
| Delimiter | message ends with `\n` | what the agent's `UnixSocketHelperDispatcher` sends today. Simple, but only safe while the payload can never contain the delimiter, and the receiver reads blindly until it finds one |
| Fixed size | every message is *N* bytes | too rigid for JSON |
| **Length prefix** | 4 bytes with the length, then the payload | the receiver knows how much to read and can **refuse before allocating** when the length exceeds the limit |

The helper is a privileged process, so it uses the length prefix: a misbehaving peer cannot make it read or
allocate without bound, and it never scans the stream looking for a delimiter.

## Wire format

```
+----------------------+---------------------------+
| length  (4 bytes,    | payload (length bytes)    |
| unsigned, big-endian)|                           |
+----------------------+---------------------------+
```

- Maximum payload: 64 KiB. A larger declared length is rejected before the payload is read.
- A zero-length frame is valid.
- End of stream before the header or before the full payload is a malformed frame.

## Why not UDP, and why not `SOCK_SEQPACKET`

- **UDP** gives neither reliable delivery nor caller identification: the kernel reports where a datagram
  came from, not who sent it.
- **`SOCK_SEQPACKET`** would give message boundaries for free on a Unix socket, but Java's
  `UnixDomainSocketAddress` is stream-only, so the agent could not use it without JNI.

So the framing is paid for in C++, and in exchange the helper gets peer credentials, per-file permissions
and reliable delivery.

## How this maps to the code

Planned layering for `adapter_socket` (a GoF Adapter hides the POSIX details; framing sits on top):

```
ISocketConnection        readExactly(n) / writeAll(bytes) / peerCredentials()
UnixSocketConnection     adapts the POSIX fd: owns it, EINTR, short reads/writes, MSG_NOSIGNAL,
                         SO_PEERCRED, timeouts
FramedConnection         4-byte big-endian prefix and the 64 KiB cap, over ISocketConnection
```

- The adapter returns **raw bytes** and knows nothing about the protocol; the size limit and byte order are
  tested against a fake `ISocketConnection` that delivers bytes in chunks, without a kernel.
- Synchronous only: the helper is one-shot and serves a single connection per activation.
- Accepting the connection from the systemd-passed fd, and where the peer check sits, are decided
  separately.

## Would ZeroMQ do this for us?

It would do the framing, but not the parts that matter most to a one-shot privileged helper, and it adds
problems of its own. Decision: hub `docs/decisions/helper-socket-posix.md` (ZeroMQ rejected).

**What ZeroMQ handles**

| Topic | ZeroMQ |
|-------|--------|
| Message boundary | yes — `zmq_recv` returns a whole message, never a fragment; ZMTP prefixes the length internally |
| Short reads, `EINTR` | yes — hidden by the library |
| Reconnect and queueing | yes |
| Messaging patterns | yes — REQ/REP, PUB/SUB, ... |

It would remove `readExactly` and `FramedConnection`.

**What it does not give the helper**

| Helper need | ZeroMQ |
|-------------|--------|
| Know **who** called (uid, gid, pid) | it can *filter* IPC peers by uid/gid/pid (`ZMQ_IPC_FILTER_UID/GID/PID`), but that option is deprecated in favour of ZAP / IPC allow-block, and the accepted peer's credentials are not handed to the application — `zmq_msg_gets` exposes `Peer-Address`, an IP address |
| Cap the size **before** allocating | only through the socket-wide `ZMQ_MAXMSGSIZE`; the rule stays in the library |
| Accept **exactly one** connection and exit | against its model (see below) |
| systemd socket activation | only through `ZMQ_USE_FD` with `Accept=no` and a stream socket (documented in libzmq) |
| Java agent | needs JeroMQ (`ipc://` support unverified) or JNI instead of the JDK's `UnixDomainSocketAddress` |

**The structural problem.** ZeroMQ accepts connections and moves data on its own I/O thread, asynchronously
to the application. A one-shot helper that serves one request and exits can have a second connection
already accepted and consumed by that thread, losing the request without the caller knowing. With a plain
`accept()` the second connection stays in the kernel backlog and triggers the next activation. The reply
also needs `ZMQ_LINGER` set and a clean context shutdown to leave the process before it exits.

| | POSIX + our framing | ZeroMQ |
|---|---|---|
| Code to maintain | `UnixSocketConnection` + `FramedConnection`, a few dozen lines of logic | less |
| Who called | `SO_PEERCRED`, direct | filter only, deprecated API |
| One-shot semantics | natural | fragile |
| Dependency | libc and libsystemd | libzmq in Yocto and in the agent |
| Testability | fake `ISocketConnection`, no kernel | real context and sockets |

ZeroMQ would save exactly the small part (framing) and cost where this layer needs control: caller identity
and process lifecycle. Revisit only if helpers need to talk to each other (pub/sub) or the agent needs to
reach many helpers concurrently.

## Related

- Hub `docs/decisions/helper-socket-posix.md` — transport, activation, peer check, frame cap.
- Hub `docs/decisions/agent-two-phase-delivery.md` — what the agent sends in each phase.
- Reference adapter: `dnsproxy/socket_client` (`ISocketClient`), the UDP counterpart this design follows.
