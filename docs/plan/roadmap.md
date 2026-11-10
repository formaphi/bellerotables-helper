# Roadmap: `bellerotables-helper` (track H / step I3)

**Snapshot:** 2026-11-04. Horizons are quarterly (Now / Next / Later). Effort sizes and dates are estimates, not
commitments, and the Later items are directional. The work items behind each cell are in
[backlog.md](backlog.md); epic numbers (E1 to E5) match it.

Sources: this repo's `CLAUDE.md` and task briefs, and in the hub (`../docs/` from the repo root):
`design/status.md`, `design/firewall-vertical-slice.md`, `design/redirection-baby-steps.md` and
`decisions/agent-two-phase-delivery.md`. The hub links back here from track H of `firewall-vertical-slice.md`.

## Where it stands

- **Done:** the sandbox build, `adapter_socket` (activation, single `accept()`, `SO_PEERCRED`, deadlines,
  framing, RAII fds) and the syscall adapters (`IUserDirectory`, `IFileReader`, `ISocketSyscalls`).
- **Done, awaiting merge into `develop`:** the request parser (E1): `contracts` pin, generated C++ and
  validators, `domain/`, `request_parser/`, and the validation decision in `docs/protovalidate-runtime.md`.
- **Parked, needed for the slice:** `adapter_iptc` (libiptc adapters including `MangleAdapter` and
  `NflogTarget`, rule builder, 50 gtest cases) on `feat/adapter-iptc`, PoC quality. The M0 `logging` rule is an
  NFLOG rule written through it.
- **Not built:** controller, M0 use case, systemd units, readback, reply path. `main.cpp` is still an empty `main`.

## What "the slice" means here

The slice is validated when **M0** of hub `docs/design/redirection-baby-steps.md` passes end
to end: a `logging`-only (or empty) rule goes PUT desired → agent → helper applies NFLOG → kernel readback →
`reported`. Real DNAT (M1a to M1c) comes after that validation.

## Themes

| Theme | Business objective | Epics |
|---|---|---|
| **T1. Helper endpoint** | Turn one frame from the agent into an NFLOG rule in the kernel. | E1, E2 |
| **T2. Reported leg** | Make `reported` come from the kernel, not from `desired`, and send it back. | E3 |
| **T3. Real redirection** | Grow the library and use case from NFLOG to a full DNAT rule. | E4 |
| **T4. Delivery** | Package the helper on the image so `EndToEnd` can run against it. | E5 |

## Roadmap (Theme × Horizon)

| Theme | **Now** (Q4 2026, to ~Dec) | **Next** (Q1–Q2 2027) | **Later** (after slice validation) |
|---|---|---|---|
| **T1. Helper endpoint** | **E1** Request parser (M–L): `contracts` submodule, protoc 25.1 codegen, `std::expected<RedirectionRequest, ParseProblem>`, protovalidate decision. **E2, first part** (M): `adapter_iptc` rebased onto `develop`, up to house rules, green under unit, ASan and Valgrind, in parallel with E1. | **E2, rest** (M): controller, M0 use case (NFLOG through `MangleAdapter`), `main.cpp` wiring, systemd units with `CAP_NET_ADMIN` only, `socat` driver (I3 exit gate). | |
| **T2. Reported leg** | | **E3** (L): readback from the kernel can start as soon as E2 applies a rule, and it informs the `reported` envelope. The reply frame and `ProblemDetails` wait on the `reported` envelope in `contracts`, agent phase 2 and the Pegasus consumer. | |
| **T3. Real redirection** | | | **E4** (L): M1a match, M1b DNAT in nat PREROUTING (a `NatAdapter`), M1c reported chain and rule number. |
| **T4. Delivery** | | **E5** (M): Yocto recipe (M4), blocked on build hardware. | `EndToEnd` `redir` suite green on the image (owned by I6). |

### Distant future (icebox)

Not on the roadmap, no dates. Revisit after the slice is validated.

- **Security hardening:** `SystemCallFilter`, `NoNewPrivileges`, a STRIDE threat model.
- **More firewall features:** source scope (M2), SNAT POSTROUTING and MARK (M3), rate-limit (M4),
  `firewall-input`.
- **Engineering health:** consumer-side CDC compile check, TSan if threading appears, `perf` once a high-load
  E2E exists.
- **`adapter_iptc` placement:** moves to `piiptables` only when a second helper needs it
  (`docs/adapter-iptc-placement.md` on `feat/adapter-iptc`).

## Epic details

| Epic | Persona | Metric it moves | Size | Dependencies and risks |
|---|---|---|---|---|
| E1 Request parser | Agent developer | Share of malformed payloads rejected without crashing (target 100%) | M–L | Needs the `contracts` pin and protobuf in the Conan file and sandbox image, then `make sandbox-build`. `CLAUDE.md` and the facade decision disagree on runtime protovalidate; settle it in `docs/` (recommendation: do not link protovalidate-cc now). |
| E2 Helper endpoint for M0 | Platform engineer | NFLOG rule present in `iptables-save` after one `socat` frame | M–L | `adapter_iptc` part has no dependency. The rest needs E1. Units are tested in the sandbox container while the recipe is blocked. |
| E3 Reported leg | Appliance operator | `reported` equals kernel state, including on partial apply | L | Readback needs only E2. The reply waits on the `reported` envelope in `contracts`, agent phase 2 and the Pegasus consumer. Inside the slice: M0's done-when needs the round trip. |
| E4 Real redirection apply | Appliance operator | A redirect actually takes effect (M1b) | L | Starts after M0 validation. Each milestone needs its contract fields first. Behavior reference: `omne-apply-firewall-redir.pl` and `suites/redir`. |
| E5 Yocto packaging | Program | Helper installed on the image with units and capability | M | Blocked on build hardware. Feeds I5. |

## Milestones and decision points

1. **M-H1 (Dec 2026):** parser merged and `adapter_iptc` on `develop`. Decision: protovalidate at runtime, yes
   or no.
2. **M-H2 (~Q1 2027):** a `socat` frame produces the NFLOG rule in the sandbox (I3 exit gate, end of E2).
   Decision: failure policy on partial apply, and readback mechanism (live libiptc or `iptables-save`).
3. **M-H3 (Q1–Q2 2027):** the reply reaches the real agent and Pegasus stores `reported` (end of E3, M0 done for
   the helper). Decision: whether the envelope moves into `contracts`.
4. **Slice validated:** M0 passes in `EndToEnd`. `develop` may merge into `master`; E4 starts.

## Resource implications

- Mostly one C++ developer, with hand-offs to `contracts`, Pegasus and the agent for the protobuf pin, the
  `reported` envelope and phase 2.
- Yocto-dependent work (E5, anything run on the image) is blocked by the local build-hardware upgrade noted in
  `status.md`.
- Adding protobuf means a sandbox rebuild now and an Abseil/protobuf recipe on Yocto later.

## Assumptions and risks

- **Assumption:** the slice is M0 (`logging` / NFLOG only); DNAT is E4, after validation.
- **Risk, schedule:** E3's reply depends on three other repos; the readback part can proceed without them.
- **Risk, correctness:** partial apply without a rollback story can leave the kernel in a state `desired` does not
  describe. `reported` must show exactly that, and the use case needs a clear failure policy.
- **Risk, dependencies:** the protobuf runtime must match `protoc` 25.1; a mismatch breaks the build or the wire.
- **Risk, `adapter_iptc` quality:** it is PoC code; bringing it up to the house rules may take longer than its
  size suggests.
- **Risk, upstream:** no delivery guarantee on the desired leg (no outbox or retry). Apply should be idempotent to
  tolerate duplicate or replayed requests.
