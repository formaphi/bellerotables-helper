# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Modular C++11 binary (`iptables-manager`) replacing legacy Perl firewall scripts (`omne-apply-firewall-input.pl`, `omne-apply-firewall-redir.pl`) from NGFW. Manages iptables rules via netfilter headers and libiptc/xtables. Target platform: AlmaLinux 8 (CentOS 7 ABI).

CLI exposes each firewall domain as a subcommand:

```bash
iptables-manager zoneProtection   # INPUT rules (zone access control)
iptables-manager portForward      # Port forwarding / DNAT
```

## Architecture

Clean Architecture with 4 concentric rings. Dependencies point inward only.

```
Ring 1 (domain)              — Pure domain logic, zero external deps
Ring 2 (application)         — Port interfaces (input + output), use cases; depends only on domain
Ring 3 (interface adapters)  — db-reader, serializer, file-writer
Ring 4 (frameworks/drivers)  — framework/db-driver, framework/command-builder, src/main.cpp
```

## Domain Model (Redir Bounded Context)

### Entities

**Policy** — sole entity and aggregate root. Identity: `redir_id` (from `firewall_redirects`). Core: id, mark, chain name. Holds feature-specific Value Objects. All iptables rules are derived output from Policy state — not entities.

### Value Objects

| VO | Purpose | Source |
|----|---------|--------|
| Device | {ip, mark} per WAN interface | `box_net_device` + `security_polices_mark` |
| MonitoringConfig | monitor/log flags, nflog prefix/group | `firewall_redirects.monitor`, `.log` |
| GeoIpConfig | countries, allowlist/blocklist mode | `firewall_redirects` + geo tables |
| DosConfig | rate, burst, hashlimit params | `firewall_redirects` + related tables |
| SslConfig | certificate, priority, workers | `firewall_redirects_ssl` + `profile_ssl` |
| SchedulingConfig | time/date constraints | `obj_time`, `obj_date` join tables |
| IpsetConfig | allowlist/blocklist IP sets | ipset tables |
| AuthConfig | session ipset, global/per-rule | `firewall_redirects` auth fields |
| IpsConfig | IPS profile, queue numbers | `profile_ips` + filesystem |
| AtpConfig | threat categories, ipset names | `firewall_redirects_threat` |

### Key Decisions

- **Policy is the only entity** — everything else is VO or external reference
- **Device is VO** — identity managed outside this bounded context
- **Iptables rules are output** — generated from Policy state, flushed and recreated each run, no identity or lifecycle
- **Feature VOs are composable** — each feature adds its VO without bloating Policy
- **External references** (IPS/SSL profiles) — managed in other bounded contexts, referenced by ID

### Execution Flow (Clean Architecture)

```
main (Ring 4) → UseCase (Ring 2) → output port interface (Ring 2)
                                      ↓ implemented by
                                   MangleAdapter (Ring 3/4)
                  ↑ receives data via input port
               DbReader (Ring 3) implements input port (Ring 2)
```

Ring 2 defines WHAT (use case + port interfaces). Ring 3 provides HOW (DB reader, adapter).

## Socket Endpoint (agent → helper)

The helper is one-shot and socket-activated: `bellero-agent` sends it one request per activation.
Transport decided in hub `docs/decisions/helper-socket-posix.md`; delivery phases in
`docs/decisions/agent-two-phase-delivery.md`.

### Wire

- systemd `.socket` unit: Unix-domain `ListenStream=`, `Accept=no`, `SocketMode=0660`, agent's group
- Helper receives the listening fd from systemd (`LISTEN_FDS`, fd 3), `accept()`s **exactly one**
  connection, serves it, exits — pending connections stay in the backlog for the next activation
- `SO_PEERCRED` on the accepted connection — refuse any uid other than the agent's
- `SOCK_STREAM`, frame = 4-byte unsigned big-endian length + payload; reject frames above the max size
  before reading them
- Payload: canonical protobuf JSON of `{correlation_id, id, desired}`
- Phase 1: no reply, close after handling. Phase 2: one reply frame on the same connection, then close

Not ZeroMQ: libzmq accepts on its own I/O thread, so a one-shot exit can drop a second queued request.
Not `SOCK_SEQPACKET`: Java's Unix-domain sockets are stream-only.

### Components

| Component | Ring | Responsibility |
|-----------|------|----------------|
| Socket lib | 4 | activation fd, single `accept()`, peer-credential check, length-prefixed read/write; RAII fds, errno → exception |
| Parser | 3 | protobuf facade (hub `protobuf-consumer-facade.md`): JSON → generated message → protovalidate → clean domain type. Outgoing (reply) serialization arrives in phase 2 |
| Controller | 3 | reads one frame, parses, calls the use case, which drives `adapter_iptc`; maps failures to the reply (phase 2) |

```
systemd fd → socket lib (Ring 4) → controller (Ring 3) → parser (Ring 3)
                                        ↓
                                   UseCase (Ring 2) → MangleAdapter (adapter_iptc)
```

Build order: socket lib → parser (incoming) → controller → reply path (phase 2).

## Build System

C++11, CMake 3.10+, Conan 2.x. GCC latest on Linux x86_64.

```bash
# Install deps (from project root)
conan install . --profile=profiles/default --build=missing

# Configure + build
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Run tests
cd build && ctest --output-on-failure
```

### Conan Profiles

- `profiles/default` — Debug build, GCC latest
- `profiles/asan` — AddressSanitizer (same settings, sanitizer flags added at CMake level)
- `profiles/tsan` — ThreadSanitizer (same settings, sanitizer flags added at CMake level)

### Dependencies (via Conan)

- `libpq/15.4` — PostgreSQL C client
- `nlohmann_json/3.11.3` — JSON (header-only)
- `gtest/1.10.0` — GoogleTest + GMock

Additionally requires system `libiptc` (netfilter).

## libiptc Usage (Ring 4)

Rules are built as a single contiguous binary buffer, not a tree of objects.

### Buffer layout

```
[ipt_entry header] [match1] [match2] ... [matchN] [target]
|                  |<-------- entry->elems[] -------->|
|<- target_offset ->|                                 |
|<- next_offset -------------------------------------------->|
```

### Flow to build a rule

1. Calculate sizes — each match + target with `XT_ALIGN(sizeof(header) + sizeof(data))`
2. Calculate `target_offset` (sum of ipt_entry + all matches) and `next_offset` (+ target)
3. Single `calloc` with `next_offset` bytes
4. Fill `ipt_entry` header (proto, src, dst)
5. For each match sequentially in the buffer:
   - Position ptr at correct offset
   - Fill `xt_entry_match` header (name, match_size, revision)
   - Fill match-specific data struct (e.g. `xt_mark_mtinfo1`, `xt_tcp`)
   - Advance `ptr += match_sz`
6. Fill target at end (e.g. NFLOG, ACCEPT, DROP, MARK)
7. `iptc_append_entry(chain, entry, handle)`
8. `iptc_commit(handle)`

### Key headers

| Header | Content |
|--------|---------|
| `<libiptc/libiptc.h>` | `iptc_init`, `iptc_create_chain`, `iptc_append_entry`, `iptc_commit`, `iptc_free` |
| `<linux/netfilter_ipv4/ip_tables.h>` | `struct ipt_entry`, `struct ipt_ip` |
| `<linux/netfilter/x_tables.h>` | `XT_ALIGN`, `xt_entry_match`, `xt_entry_target` |
| `<linux/netfilter/xt_mark.h>` | match mark (`xt_mark_mtinfo1`, rev=1) |
| `<linux/netfilter/xt_tcpudp.h>` | match tcp (`xt_tcp`, rev=0) — `spts[2]` and `dpts[2]` are separate fields |
| `<linux/netfilter/xt_conntrack.h>` | match conntrack (`xt_conntrack_mtinfo3`, rev=3) |
| `<linux/netfilter/xt_hashlimit.h>` | match hashlimit (`xt_hashlimit_mtinfo1`, rev=1) — `gc_interval` must be > 0 |
| `<linux/netfilter/xt_NFLOG.h>` | target NFLOG (`xt_nflog_info`, rev=0) |

### Target types

Two distinct binary layouts — standard vs extension:

| Type | Struct | Name field | Data |
|------|--------|-----------|------|
| Standard (ACCEPT, DROP, RETURN) | `xt_standard_target` | empty | `int verdict` |
| Extension (NFLOG, DNAT, MARK…) | `xt_entry_target` + data | filled | type-specific struct |

Standard target verdicts: `ACCEPT = -NF_ACCEPT - 1`, `DROP = -NF_DROP - 1`, `RETURN = XT_RETURN`.

`iptc_get_target()` returns empty string for standard targets — read `xt_standard_target.verdict` to identify.

### Design patterns (Ring 4 wrapper)

```
Match (Template Method)
  write(): set match_size → set name → set revision → fillData()
  Subclasses: MarkMatch, TcpMatch, UdpMatch, ConntrackMatch, HashlimitMatch
  Protocol match (tcp/udp) declares protocol() → Builder infers ipt_entry.ip.proto

ITarget (Strategy interface)
├── StandardTarget — enum class Verdict { ACCEPT, DROP, RETURN }
└── ExtensionTarget (Template Method)
    write(): set target_size → set name → fillData()
    Subclasses: NflogTarget, (future: DnatTarget, MarkTarget…)

RuleBuilder (Builder)
  addMatch() → setTarget() → build()
  Owns buffer allocation (calloc) and offset calculation
```

Files: `iptc_builder.h` (Match + RuleBuilder), `iptc_target.h` (ITarget hierarchy).

### Gotchas

- Link with `ip4tc` (not `iptc`)
- AlmaLinux 8 defaults to nftables backend — `iptables` CLI won't see legacy tables written by libiptc. Use libiptc reader functions to verify.
- `-fpermissive` needed for AlmaLinux 8 kernel headers (void* cast in `ipt_get_target`)
- `hashlimit` `cfg.gc_interval` must be > 0 (e.g. 1000ms), kernel rejects 0
- `hashlimit` `cfg.avg` for `--hashlimit-upto`: `XT_HASHLIMIT_SCALE * seconds_per_unit / rate` (12/hour → `scale * 300`)
- C++ requires explicit cast from `calloc` return (`void*` → `ipt_entry*`), use `unique_ptr` with `FreeDeleter` for RAII
- `unsigned char *` required for buffer pointers — binary data with nulls, not text. `std::string`/`shared_ptr` not applicable.

### Docker dev environment

```bash
make up        # build image + start container
make enter     # shell into container
make down      # remove container
make rebuild   # down + up
```

Binary at `/build/out/iptables-manager` inside container. Requires `--cap-add NET_ADMIN`.

#### Verifying rules written by libiptc

AlmaLinux 8 `iptables` CLI uses `xtables-nft-multi` (nftables backend). libiptc writes to legacy tables — the default CLI cannot see them. The Dockerfile builds `iptables-legacy` from source and installs it at `/usr/local/sbin/iptables-legacy`.

```bash
# Inside container — verify rules written by libiptc
XTABLES_LIBDIR=/usr/lib64/xtables iptables-legacy -t mangle -S bb_mangle_redir_1
```

`XTABLES_LIBDIR` is also set system-wide via `/etc/profile.d/xtables.sh` (active in login shells).

> **Note for EndToEnd:** the legacy sandbox verified rules through `iptables-restore` (nft backend). Once the helper (libiptc) writes the rules, the EndToEnd firewall container needs the same `iptables-legacy` build to inspect them.

## End-to-End Tests

Acceptance tests live outside this repo, in the hub's `EndToEnd` submodule (`hub-bellerophon/EndToEnd`): a Python behave harness forked from the legacy end2end suite, legacy Gherkin kept as `@pending`. Scenarios exercising this helper are reactivated once the socket endpoint and `bellero-agent` are in place. See `EndToEnd/docs/specs/` for the harness.
