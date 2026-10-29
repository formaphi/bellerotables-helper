# CLAUDE.md — bellerotables-helper

Guidance for Claude Code working in this repo.

## What this repo is

The **OS mutator** for the firewall domain: a **C++ one-shot** binary, systemd **socket-activated**,
that applies iptables rules then **reads the OS back** and returns the real kernel state, then exits.
Track **H** of the firewall vertical slice. Runs with **minimum capability** (`CAP_NET_ADMIN` only —
not blanket root).

The one rule that cannot slip: `reported` must come from **reality** (`iptables-save` / live readback),
never an echo of `desired`. Partial apply → reported shows exactly what stuck.

**No DB, no network, no redis.** The helper talks **only** to the agent, locally, over a **unix
socket** (one-shot exec + stdin/stdout). The agent validates + correlates + forwards; the API writes.

## Sibling repos (same parent `hub-bellerophon/`)

| Repo | Role | Relation |
|------|------|----------|
| `contracts` | Protobuf SSOT + codegen | **pinned submodule**; C++ classes generated from it |
| `Pegasus` | rest-api + agent (Java) | the agent dispatches to this helper over a unix socket |
| `meta-bellerophon` | Yocto integration | packages this helper into the proto image (I5) |

**Dependencies:** `contracts` (C++ codegen). This repo is integration step **I3**.

## Ships a new C++ contract-SDK lib

This helper carries a **greenfield** C++ SDK lib for JSON parsing/normalization + the per-domain
contract plumbing. It is **NOT** the legacy `lib-omnecpp` (that is crypto/auth coupled to
Postgres/PAM — `OmneAuth`/`OmneCrypto`, links `-lcrypto -lpq`, zero JSON — unrelated). Do not reuse it.

## Spec + plan

**Not written yet.** Author track-H spec → plan → build **in this repo** (`docs/`). Architecture
rationale is in the hub: `../docs/decisions/apply-bellerophon-architecture.md` (helper role +
security table). The legacy behavior reference is the Perl apply script
`omne-apply-firewall-redir.pl` and the `suites/redir/` E2E suite in `~/development/end2end`.

## Locked constraints

- **One-shot, socket-activated, `CAP_NET_ADMIN` only.** Apply → readback → reported → exit.
- **Consumes protobuf C++ classes** generated from `contracts` (pinned submodule, CMake custom
  target runs `codegen/gen.sh`, `protoc --cpp_out`) — confined behind this repo's own nlohmann-based
  facade, per [[protobuf-consumer-facade]]; nlohmann is this repo's parsing layer, not what's
  generated from the contract.
- **Reported = OS truth**, never an echo of desired.
- Slice scope: `redirection` DNAT in nat PREROUTING; SNAT POSTROUTING placement deferred from
  reported (see contract design M3).

## Contract testing (post-slice)

Future improvement, **not in the current slice**. Cross-cutting strategy owned by the program hub:
`../docs/decisions/consumer-driven-contract-testing.md`. This repo's CDC responsibility when it lands:

- **Consumer structural** — regenerate the C++ protobuf headers from the pinned `contracts` SHA
  (CMake `add_custom_command`) and compile; a breaking schema change surfaces as a compile break.
  The generated classes are the pact, the compiler is the verifier. The helper validates nothing at
  runtime — it trusts input already validated by the API upstream.

## Socket Endpoint (agent → helper)

The helper is one-shot and socket-activated: `bellero-agent` sends it one request per activation.
Transport decided in hub `docs/decisions/helper-socket-posix.md`; delivery phases in
`docs/decisions/agent-two-phase-delivery.md`. Why the wire is a length-prefixed stream, and how a Unix
stream socket differs from UDP: [docs/socket-stream-and-framing.md](docs/socket-stream-and-framing.md).
The helper is the socket **server** (systemd listens, the helper `accept()`s one connection) and the agent
is the client: [docs/socket-roles.md](docs/socket-roles.md). The library's single entry is
`ISocketEndpoint::acceptAuthorized()` (activation + accept + peer check → `IFramedConnection`), decided in
[docs/socket-endpoint.md](docs/socket-endpoint.md) — accepted and implemented. Every fd is owned by a
`FileDescriptor` and moves as `std::unique_ptr<IFileDescriptor>` from activation to listener to connection:
[docs/socket-file-descriptor.md](docs/socket-file-descriptor.md). Where the raw POSIX calls (`getpwnam_r`,
`recv`, `accept4`, …) should live — one small adapter interface per kind of syscall, **proposed, not
implemented** — is in [docs/syscall-adapters.md](docs/syscall-adapters.md). The `LEARN` / `OPTIMIZE`
annotations still in the sources, and the follow-up for each, are listed in
[docs/socket-code-notes.md](docs/socket-code-notes.md).

### Wire

- systemd `.socket` unit: `ListenStream=/run/bellerophon/bellerotables-helper.sock`, `Accept=no`,
  `SocketMode=0660`, agent's group
- Helper takes the listening fd with `sd_listen_fds()` (libsystemd), `accept()`s **exactly one**
  connection, serves it, exits — pending connections stay in the backlog for the next activation
- `SO_PEERCRED` on the accepted connection — refuse any uid other than the agent's. The agent's user
  comes from `/etc/bellerophon/bellerotables-helper.conf` (`agent_user=bellero-agent`, `root:root 0644`),
  resolved with `getpwnam()` at startup; missing file or unknown user refuses every connection
- `SOCK_STREAM`, frame = 4-byte unsigned big-endian length + payload; frames above 64 KiB are rejected
  before reading the payload
- Payload: canonical protobuf JSON of `{correlation_id, id, desired}`
- Phase 1: no reply, close after handling. Phase 2: one reply frame on the same connection, then close

Not ZeroMQ: libzmq accepts on its own I/O thread, so a one-shot exit can drop a second queued request.
Not `SOCK_SEQPACKET`: Java's Unix-domain sockets are stream-only.

### Components

| Component | Ring | Responsibility |
|-----------|------|----------------|
| Socket lib (`adapter_socket/`) | 4 | activation fd, single `accept()`, peer-credential check, length-prefixed read/write; RAII fds, errno → exception |
| Parser | 3 | protobuf facade (hub `protobuf-consumer-facade.md`): JSON → generated message → protovalidate → clean domain type. Outgoing (reply) serialization arrives in phase 2 |
| Controller | 3 | reads one frame, parses, calls the use case, which drives `adapter_iptc`; maps failures to the reply (phase 2) |

```
systemd fd → socket lib (Ring 4) → controller (Ring 3) → parser (Ring 3)
                                        ↓
                                   UseCase (Ring 2) → MangleAdapter (adapter_iptc)
```

Build order: socket lib → parser (incoming) → controller → reply path (phase 2).

## Build

C++23, CMake 3.28+, Conan 2.x, GCC 13 (same major as Yocto Scarthgap; `std::expected` needs 12+). Every
build runs inside the sandbox container (`sandbox/`, Ubuntu 24.04), driven by the `Makefile`. The build
infrastructure was ported from the personal `piiptables` repo.

```bash
make sandbox-build                   # build the image (pre-builds Conan deps)
make rebuild                         # Release binary, BUILD_TESTING=OFF -> production-builds/release/
make rebuild-unit && make test-unit  # tests + gcov -> coverage-reports/html/index.html
make rebuild-asan && make test-asan  # tests under ASan + UBSan
make rebuild-valgrind && make test-valgrind
make rebuild-tsan && make test-tsan
make test-one-asan PATTERN=<regex>   # one test binary; same for -unit, -tsan, -valgrind
make sandbox-shell                   # interactive shell, repo mounted at /workspace
make help                            # every target
```

- One Conan profile, `profiles/gcc13.conan` (`build_type=Release`); sanitizer and coverage flags are added at
  the CMake level so all build types reuse the dependencies cached in the image.
- Release output lives on the host in `production-builds/`; test builds live in the `test_builds` Docker
  volume. The container runs as root with `cap_drop: ALL` + `NET_ADMIN`/`NET_RAW`, so it cannot write
  host-owned dirs: the Makefile pre-creates output dirs world-writable and deletes through the container.
- Each library (`adapter_*/`) includes its `test/` subdirectory only under `if(BUILD_TESTING)`;
  `format` and `tidy` pick up every `adapter_*/` directory.
- TSan aborts with `unexpected memory mapping` on hosts with `vm.mmap_rnd_bits=32` (Ubuntu 24.04 kernels);
  the container's seccomp profile blocks `setarch -R`. Run `sudo sysctl vm.mmap_rnd_bits=28` on the host
  before `make test-tsan`. No helper code is multithreaded yet, so TSan is out of the pre-commit hook.
- `make setup` activates `.githooks/pre-commit` (sandbox image, `format-check`, release, `tidy-check`, unit,
  ASan, Valgrind). `format` and `tidy-fix` rewrite sources, so they run as the host user.
- How each build type works and its pitfalls: `docs/build/` (asan, tsan, valgrind, gcov, clang-format,
  clang-tidy). Profiling with `perf` is deferred until a high-load E2E scenario exists; the options are in
  `docs/build/perf.md`.
- The sandbox sets `iptables` to the legacy backend (`update-alternatives`), so the CLI reads the tables
  libiptc writes.

> **Transitional.** The sandbox stands in for the `meta-bellerophon` image until that image can be
> dockerized (hub `docs/design/firewall-vertical-slice.md`, decision 8). Keep it close to the target:
> same GCC major, legacy iptables, libsystemd.

### Dependencies

- Conan: `nlohmann_json/3.11.3` (linked PUBLIC by `adapter_socket`; used by the parser next),
  `gtest/1.14.0` (test requirement; GTest + GMock). Adding a Conan dependency needs `make sandbox-build`: the
  image pre-builds the cache and `conan install` runs with `--no-remote`
- System (apt, in the image): `libiptc-dev` (link `ip4tc`), `libxtables-dev`, `libsystemd-dev` (for
  `sd_listen_fds()`), legacy `iptables` as the default alternative
- Coming with the parser: protobuf (version must match the `protoc` used by `contracts`)

## Code style

- Comments are allowed, but every comment must start with a comment modifier: `BUG`, `COMPLEX`, `FIXME`, `HACK`,
  `LEARN`, `OPTIMIZE`, `REVIEW` or `TODO`, followed by a colon and a description. The modifiers and their meanings
  are defined in the hub `CLAUDE.md` (Comment modifiers); do not redefine them here.
- The modifiers are what [Todo Tree](https://github.com/Gruntfuggly/todo-tree) lists in the editor. Write the
  comment as a `//` line, or a `/* MODIFIER: ... */` block when it needs several lines, with the modifier on the
  first line. Modifier comments stay in the code when merging into `develop`.
- Do not commit unless asked. Branch before a PR.

## Branching

- Feature branches are cut from `develop` and squash-merged into `develop`.
- For the first vertical slice everything lands on `develop`; `develop` merges into `master` only after the
  slice is validated end to end by `EndToEnd` (hub `CLAUDE.md`, Working mode).
