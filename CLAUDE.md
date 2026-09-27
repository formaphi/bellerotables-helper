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

## Code style

- No comments/docstrings in code unless requested; explain via names.
- Do not commit unless asked. Branch before a PR.
