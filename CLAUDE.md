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

## End-to-End Tests (submodule)

The `end2end/` submodule contains BDD characterization tests for the legacy Perl scripts, using Python Behave + Docker sandbox.

```bash
cd end2end
python -m venv venv && source venv/bin/activate && pip install -r requirements.txt

make up                              # start Docker sandbox
make features                        # run all BDD scenarios
make feature name=geoip              # run single feature
make tags tag=@wip                   # run by tag
make unit-test                       # pytest unit tests (no Docker)
make down                            # stop sandbox
```

Tests run from host (not inside containers). They use `docker exec` for firewall commands and connect to DB on `localhost:5432`. See `end2end/CLAUDE.md` for full details.
