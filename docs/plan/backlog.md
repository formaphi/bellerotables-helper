# Backlog: `bellerotables-helper` (track H / step I3)

**Snapshot:** 2026-11-04. Companion to [roadmap.md](roadmap.md).
`adapter_socket` and the syscall adapters are done (`feat/syscall-adapters`) and are not in this backlog.

Points are relative (1, 2, 3, 5, 8); anything above 8 must be split before it enters a sprint. Order is priority:
the first item is done first. Only E1 is refined to story level; the other epics list their stories by title and
get acceptance criteria when they move into the top two sprints.

Milestones (M0, M1a, ...) are those of hub `docs/design/redirection-baby-steps.md`. The
slice is validated when M0 passes end to end; real DNAT comes after.

## Epics

| # | Epic | When | MoSCoW | State |
|---|---|---|---|---|
| E1 | Request parser (protobuf facade) | Now | Must | Done (H-101 to H-105), awaiting merge into `develop` |
| E2 | Helper endpoint for M0 (`adapter_iptc`, controller, NFLOG apply, units, driver) | Next | Must | Outline |
| E3 | Reported leg (readback and reply) | Blocked on the agent, Pegasus and `contracts` | Must | Outline |
| E4 | Real redirection apply (M1a to M1c) | After slice validation (M0) | Must | Outline |
| E5 | Yocto packaging (M4 in `meta-bellerophon`) | Blocked on build hardware | Should | Outline |

Not epics, now stories in E2: the `adapter_iptc` rebase, the systemd units and the manual `socat` driver.
`adapter_iptc` is mandatory for the slice: the M0 `logging` rule is an NFLOG rule written through libiptc
(`MangleAdapter`, `NflogTarget`), not a no-op.

## E1. Request parser

**Goal:** turn the payload of one request frame into a clean domain request or a typed failure, with protobuf
confined to one library.
**Brief:** `bellerotables-helper/task-request-parser.md`.

| ID | Story | Pts |
|---|---|---|
| H-101 | Pin `contracts` and generate the C++ classes in the build | 5 |
| H-102 | Parse a valid envelope into `RedirectionRequest` | 3 |
| H-103 | Reject bad input without crashing | 5 |
| H-104 | Record the protovalidate decision | 2 |
| H-105 | Keep protobuf out of public headers | 1 |

### H-101. Pin `contracts` and generate C++ in the build (5)

As a helper developer, I want the C++ classes regenerated from the pinned `contracts` at every build, so that a
breaking schema change fails to compile.

- AC1: Given a fresh clone, When I run `git submodule update --init` and `make rebuild`, Then the binary builds
  with no manual step.
- AC2: Given a clean build, When it finishes, Then the `.pb.h` and `.pb.cc` files exist in the build directory
  and `git status` shows no generated file.
- AC3: Given `Logging.group` is renamed in a scratch copy of the submodule, When I run `make rebuild-unit`, Then
  the compile fails in the mapping code.
- AC4: Given the sandbox image, When it is rebuilt, Then `protoc` is 25.1 and the linked `libprotobuf` matches it.

Depends on: none. Needs `make sandbox-build` after the Conan and image changes.

### H-102. Parse a valid envelope (3)

As the controller, I want `std::expected<RedirectionRequest, ParseProblem>` from the payload text, so that a use
case can run on clean values.

- AC1: Given `{"correlation_id":"<uuid>","id":7,"desired":{"logging":{"group":5}}}`, When parsed, Then `id` is 7,
  `group` is 5, and `prefix`, `range`, `threshold` are empty.
- AC2: Given the same payload with `prefix`, `range` and `threshold` set, When parsed, Then all four values are
  carried.
- AC3: Given `"desired":{}`, When parsed, Then the request has an empty `Logging` (logging disabled).

Depends on: H-101.

### H-103. Reject bad input without crashing (5)

As a security reviewer, I want every invalid payload to come back as a value, so that untrusted input never
crashes or throws in a privileged process.

- AC1: Given an empty string, truncated JSON, or non-UTF-8 bytes, When parsed, Then the kind is malformed JSON.
- AC2: Given an envelope without `correlation_id`, without `id`, or without `desired`, When parsed, Then the kind
  is missing envelope field (three cases).
- AC3: Given `"id":"7"`, `"desired":[]`, or `"group":"5"`, When parsed, Then the kind is wrong field type.
- AC4: Given an unknown key inside `desired` or inside `logging`, When parsed, Then the kind is unknown field.
- AC5: Given 64 KiB of valid JSON padding and a deeply nested payload, When parsed, Then there is no crash and no
  unbounded recursion.
- AC6: Given any failure, When I read the detail, Then it contains at most the field name and a short reason.
- AC7: Given the ASan and Valgrind builds, When the tests run, Then there are no errors or leaks.

Depends on: H-102.

### H-104. Record the protovalidate decision (2)

As the team, I want one written decision on runtime protovalidate, so that `CLAUDE.md` and the facade decision
stop contradicting each other.

- AC1: Given the decision is made, When I open `docs/`, Then a short document states the choice, the reason and
  what would reverse it.
- AC2: Given the document, When I read `CLAUDE.md` and `protobuf-consumer-facade.md`, Then neither contradicts it
  (the hub file gets only a pointer or status line).
- AC3: Given the choice is "do not link protovalidate-cc" (the recommendation), When `group = -1` is parsed, Then
  a test asserts it is accepted by this layer.

Depends on: none. It can run in parallel with H-101.

### H-105. Keep protobuf out of public headers (1)

As a maintainer, I want one blast radius for the heavy dependency, so that only the parser library links
`libprotobuf`.

- AC1: Given the repo, When I grep for `.pb.h` and `google/protobuf` outside `contracts`, Then hits are only under
  the parser's `src/` and its tests.
- AC2: Given the CMake files, When I grep for `protobuf`, Then the link appears only on the parser target, and
  `adapter_socket` still builds without it.
- AC3: Given the new folder, When I run `make format-check` and `make tidy-check`, Then both report nothing.

Depends on: H-102.

## E2. Helper endpoint for M0

**Goal:** the helper accepts one request from the agent, applies the M0 `logging` rule (NFLOG) through libiptc,
and exits, so the M0 pipe can be driven by hand. Phase 1: no reply, close after handling.
**Depends on:** E1 for H-204 onwards. H-201 to H-203 are independent of E1 and can run in parallel with it.

| ID | Story | Pts |
|---|---|---|
| H-201 | Rebase `feat/adapter-iptc` onto `develop` (the 50 existing cases pass, `adapter_socket` tests unchanged) | 3 |
| H-202 | Bring `adapter_iptc` up to the `adapter_socket` house rules (headers under `include/`, libiptc behind a mockable adapter as in `docs/syscall-adapters.md`, format and tidy clean) | 5 |
| H-203 | Run `adapter_iptc` under unit, ASan and Valgrind with no errors or leaks | 2 |
| H-204 | Controller: read one frame, parse it, call the use case, handle a parse failure | |
| H-205 | Use case for M0: `logging` present gives an NFLOG rule (group, prefix, range, threshold) through `MangleAdapter`; `logging` absent removes it | |
| H-206 | Wire `main.cpp`: build the adapters with `SystemAdapters::createPosix()`, run the controller, exit | |
| H-207 | systemd `.socket` and `.service` units: `Accept=no`, mode 0660, agent group, `CAP_NET_ADMIN` only; tested in the sandbox container | |
| H-208 | Manual driver: a `socat` script sends one frame and `iptables-save` shows the NFLOG rule (I3 exit gate) | |

## E3. Reported leg (readback and reply)

**Goal:** `reported` comes from the OS, never an echo of `desired`, and travels back to the agent.
**Depends on:** the agent's phase-2 design (`bellero-agent`), the `reported` consumer in Pegasus, and the
`reported` envelope in `contracts` (the `reported` and `problem` bodies exist; the envelope does not). None of
these is in this repo.
**Note:** per hub `agent-two-phase-delivery.md`, the order is helper apply endpoint → `reported` envelope →
agent phase 2 + Pegasus consumer, and "the helper drives the return contract". The readback itself (H-301,
H-302) only needs the kernel, so it can start before the dependencies and inform the envelope design. The
reply (H-303, H-304) waits on them. Phase 2 is inside the slice: M0's done-when needs the round trip.

| ID | Story |
|---|---|
| H-301 | Readback: read the live kernel state and build `reported` from it |
| H-302 | Partial apply: `reported` shows exactly what stuck, with a stated failure policy |
| H-303 | Reply path: one reply frame on the same connection, then close |
| H-304 | `ProblemDetails` on the wire for failures |

## E4. Real redirection apply

**Goal:** M1a (match), M1b (DNAT target) and M1c (reported identity and placement) on real iptables.
**Starts:** after M0 is validated end to end. Behavior reference: `omne-apply-firewall-redir.pl` and
`suites/redir`.

| ID | Story |
|---|---|
| H-401 | M1a: build the match (`-p tcp --dport N`) |
| H-402 | M1b: a `NatAdapter` in `adapter_iptc` that emits DNAT in nat PREROUTING (`-j DNAT --to-destination addr:port`) |
| H-403 | M1c: report the real chain and rule number |

Stories wait for the contract fields of each milestone.

## E5. Yocto packaging

**Goal:** recipe, units and capability for the helper in `meta-bellerophon` (M4).
**Blocked:** the local build hardware (see `status.md`). Feeds I5.

## Distant future (icebox)

Not planned, no stories. Revisit after the slice is validated.

- **Security hardening:** `SystemCallFilter`, `NoNewPrivileges`, a STRIDE threat model.
- **More firewall features:** SNAT POSTROUTING (M3), `firewall-input`, source scope (M2), rate-limit (M4).
- **Engineering health:** consumer-side CDC compile check, TSan if threading appears, `perf` once a high-load
  E2E exists.

## Prioritization

| Rank | Item | Why |
|---|---|---|
| 1 | H-101 | Everything downstream needs the generated classes; it also carries the dependency risk. |
| 2 | H-104 | Cheap, and H-103's rule-violation behavior depends on it. Runs in parallel with H-101. |
| 3 | H-102, H-103, H-105 | Complete the parser. |
| 3 | H-201 to H-203 | `adapter_iptc` on `develop`; independent of the parser, so in parallel with it. |
| 4 | H-204 to H-208 | After E1 and H-203; ends with the I3 exit gate. |
| 5 | H-301, H-302 | Readback can start once H-205 applies a rule; it informs the `reported` envelope. |
| - | H-303, H-304, E5 | Waiting on other repos and hardware. |
| - | E4 | Waiting on M0 validation. |

## Open questions

- **Failure policy on partial apply** (H-302): stop at the first failure, continue, or roll back. It decides what
  `reported` means after a failure.
- **Readback mechanism** (H-301): live libiptc versus parsing `iptables-save`.
- **Envelope type:** keep ad-hoc JSON or move it to a `contracts` message.
