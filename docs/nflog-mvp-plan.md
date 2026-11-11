# NFLOG MVP: add/remove/read the `logging` rule — review + plan

**Status:** proposed 2026-11-18, not implemented. This is the plan asked for before touching code.
**Scope:** close the loop the README promises — `bellero-agent` sends `{correlation_id, id, desired}`,
the helper writes the NFLOG rule for `desired.logging` through libiptc, reads the kernel back, and the
result is a value the (future) controller can log and eventually reply with. Table/DNAT/mark wiring for
the rest of `redirection` (M1+) stays out.
**Background:** [request-controller.md](request-controller.md) (H-204/H-205/H-206 backlog, Ring model),
[adapter-iptc-placement.md](adapter-iptc-placement.md), [adapter-iptc-decision-trail.md](adapter-iptc-decision-trail.md),
[syscall-adapters.md](syscall-adapters.md), `contracts/CLAUDE.md` (schema boundary rule).

## Implementation status (2026-11-18): `adapter_iptc` only

Scope narrowed to `adapter_iptc`; `domain`, `redirection_use_case`, `controller` and `main.cpp` come
later. Existing interfaces and classes kept; only additions:

| Change | Where |
|---|---|
| `clang-format` + quoted project includes (the library failed `format-check`/`include-check`) | whole library |
| `IIptcHelper::chainExists()`, `IIptcHelper::listEntries()` (owned copies, rule order, `IptEntryPtr` = `unique_ptr<ipt_entry, decltype(&std::free)>`) | `i_iptc_helper.h`, `IptcHelper`, `MockIptcHelper` |
| `IMangleAdapter::readNflogRules()` returning `NflogRuleParam` (mark + NFLOG knobs); `createChain()` now skips an existing chain | `i_mangle_adapter.h`, `MangleAdapter`, `MockMangleAdapter` |
| `NflogParam::range`/`threshold` (+ `operator==`); `NflogTarget(NflogParam)`; `NflogTarget::read(const ipt_entry&)`; `addNflogRule` no longer drops them | `dtos/nflog.h`, `targets/nflog_target.*` |
| `range` written as `len` + `XT_NFLOG_F_COPY_LEN` (`--nflog-size`); `--nflog-range` is a no-op in the kernel | `nflog_target.cpp` |
| prefix cut to 63 bytes on a UTF-8 character boundary | `nflog_target.cpp` |
| bounds-checked decoding as private `dataOf()` of the class that writes the layout (`NflogTarget`, `MarkMatch`); the base templates stay write-only | `nflog_target.*`, `mark_match.*` |
| **fix:** `commit()` leaked the libiptc handle (`iptc_commit` does not free it; 63 KB over the kernel suite under LSan) | `iptc_helper.cpp` |
| `IptcException::errorNumber()`; messages name the failing call (`iptc_init(mangle): ...`) | `iptc_exception.h`, `iptc_helper.cpp` |
| `MarkParam` moved to its own DTO `dtos/mark.h` (`nflog.h` includes it); `MarkMatch::read()` decodes a plain mark match | `dtos/mark.h`, `matchs/mark_match.*` |
| `IBuilder::build()`/`RuleBuilder::build()` return `IptEntryPtr` (ownership of the new rule); alias in its own header | `ipt_entry_ptr.h`, `i_iptc_builder.h`, `builder/rule_builder.*`, `MockBuilder` |
| include guards on `iptc_exception.h`, `dtos/nflog.h`; `IptcHelper` not copyable | headers |
| kernel tier `adapter_iptc_kernel_tests` (label `kernel`): real libiptc in a private network namespace, skips when it cannot isolate | `test/kernel/` |

Note for `adapter-iptc-decision-trail.md`: its "iptc_commit releases the handle" row is not true for
libiptc 1.8.10 (verified with LeakSanitizer).

Still open: `make tidy-check` fails on `adapter_iptc`, as it did before this change. The rest of the
warnings are either signature changes to existing classes (`[[nodiscard]]` on interface methods,
swappable/pass-by-value constructor parameters) or `reinterpret_cast`/union access, which decoding
netfilter buffers cannot avoid and which needs a policy (for example a scoped
`adapter_iptc/.clang-tidy`).

## How this review was done

Built the whole tree outside the documented Docker sandbox (no Docker available here): restored
`vendor/conan/conan-cache.tgz` with `conan cache restore` (it is a `conan cache save` archive, not a raw
tarball of `~/.conan2` — extracting it elsewhere needs `conan cache restore`, not `tar`), extracted
`vendor/apt/*.deb` with `dpkg-deb -x` into a scratch sysroot (no root available either), and pointed
`PKG_CONFIG_PATH`/`CPLUS_INCLUDE_PATH`/linker flags at it. `cmake --build` succeeds and **all 10 existing
test binaries pass** (`domain`, `adapter_socket`, `contracts_cpp` + the `bvalidate` fixture tests,
`request_parser`, `adapter_iptc`). This matches the repo's own claim: the baseline is green.

Then, to check the libiptc code actually talks to a kernel (not just that it builds), I used
`unshare --user --map-root-user --net` to get an isolated, unprivileged network namespace with a working
`mangle` table, and linked `adapter_iptc`'s real sources (no mocks) into a throwaway `main()`:

- `MangleAdapter::createChain()` + `addNflogRule(MarkParam, NflogParam)` + `commit()` really writes a
  `mark` + `NFLOG` rule; `iptables-legacy -t mangle -L -v` shows exactly what the CLI would produce from
  the equivalent `-m mark --mark … -j NFLOG --nflog-group … --nflog-prefix …`.
- Reading it back with `iptc_first_rule`/`iptc_next_rule`/`iptc_get_target` and reinterpreting
  `xt_entry_target::data` as `xt_nflog_info` recovers `group` and `prefix` correctly.
- Calling `createChain()` a second time on the same chain **throws** `IptcException("Chain already
  exists")` — confirmed, not assumed.

This namespace trick needs `CAP_SYS_ADMIN` *somewhere* in the privilege chain (either real root, or an
unprivileged user namespace the kernel allows). It worked in this sandbox; the project's Docker sandbox
runs `cap_drop: ALL` + `cap_add: [NET_ADMIN, NET_RAW]`, which does **not** include `CAP_SYS_ADMIN` — so
whether the same trick works there depends on whether the container can still create a nested user
namespace (Ubuntu 24.04's AppArmor can restrict that). Flagged below as something to confirm in the real
sandbox, not assumed.

## Current state (what's actually true today)

| Piece | State |
|---|---|
| `contracts` (`Logging`, `DesiredRedirection`, `Accepted`) | done, stable |
| `request_parser` | done: JSON → validated `domain::RedirectionRequest`, thoroughly tested (valid/rejected/leak cases), protobuf confined to the `.cpp` |
| `domain` | `Logging`, `DesiredRedirection`, `RedirectionRequest`, `IRedirectionUseCase`, `RedirectionProblem` exist with mocks and tests. **No concrete use case.** No `ReportedRedirection` yet (only in `contracts`, unmirrored) |
| `adapter_iptc` | builds, real kernel writes confirmed above, decent unit coverage for buffer layout (`RuleBuilder`, matches, targets). **No read path, no production wiring, several gaps below** |
| `adapter_socket` | done, tested, matches `docs/socket-*.md` exactly (`ISocketEndpoint`, `IFramedConnection`, exceptions) |
| `controller/` (Ring 3, H-204) | **does not exist** |
| `adapter_logger/` (Ring 4) | **does not exist**; `spdlog` is not in `conanfile.py` nor in `vendor/conan` |
| `main.cpp` | empty (`return 0;`) |

So the gap between "PoC" and "MVP that adds/removes/reads the logging rule" is: a concrete use case,
read-back, and enough of `adapter_iptc` hardening to make that use case correct — `controller`/`main.cpp`
wiring is a separate, already-planned step (H-206) that can follow once the use case exists.

## Defects / gaps found in `adapter_iptc` (confirmed, not guessed)

1. **`createChain()` is not idempotent.** Verified above: a second `apply()` for the same `id` throws.
   The use case will call this on every activation (PUT is "set desired state", not "first time only").
2. **No read path.** `IIptcHelper`/`IMangleAdapter` can create, flush, append, commit — nothing reads a
   chain back. `iptc_first_rule`/`iptc_next_rule`/`iptc_get_target` are unused. Required for "read
   firewall rules" and for the README's non-negotiable rule ("reported is kernel reality, never an echo
   of desired").
3. **`NflogTarget` only carries `group` and `prefix`.** The contract's `Logging` also has `range`
   (`--nflog-range`, kernel field `len` + `XT_NFLOG_F_COPY_LEN` flag) and `threshold` (kernel field
   `threshold`). `xt_nflog_info` has both; `NflogTarget` writes neither.
4. **Prefix byte budget vs. the contract's character budget.** `buf.validate` on `Logging.prefix` is
   `string.max_len = 64`, which protovalidate counts in **Unicode characters**; `request_parser`'s own
   test `PrefixOf64MultibyteCharacters` accepts a 128-byte prefix (64× 2-byte UTF-8). The kernel struct is
   `char prefix[64]` — 63 usable bytes. `NflogTarget::fillData` does
   `strncpy(info->prefix, m_prefix.c_str(), sizeof(info->prefix) - 1)`, which silently truncates and can
   cut a multi-byte UTF-8 sequence in half, producing invalid UTF-8 in what we read back. This is a real
   bug path, not a corner case invented for this doc — request_parser already proves the contract allows
   an input that the kernel cannot hold as given.
5. **No include guards** in `exceptions/iptc_exception.h` and `dtos/nflog.h` (confirmed by `head`).
6. **Everything in `adapter_iptc` is in the global namespace** (`IBuilder`, `IMangleAdapter`,
   `TemplateMatch`, …) — the only library in the repo without a namespace. Harmless today; a latent
   collision risk once it sits in the same translation units as `domain`/`controller`/`adapter_logger`
   types in `main.cpp`'s object graph.
7. **`-fpermissive`** is set on the whole library and its tests (`adapter_iptc/CMakeLists.txt`,
   `test/CMakeLists.txt`). It compiles today; worth a follow-up to find out *what* it is silencing (likely
   a kernel-uapi-header-in-C++ issue) and scope the flag down, instead of carrying a blanket relaxation of
   `-Werror`-adjacent checks into new code.
8. **`.clang-tidy`'s `HeaderFilterRegex`** only covers `adapter_iptc|adapter_socket|domain|request_parser`
   — any new library (`redirection_use_case`, `adapter_logger`, `controller`) needs adding or `tidy-check`
   silently skips it.

None of these are reasons not to proceed — they are exactly the kind of thing "review before
implementing" should surface, and all are small, mechanical fixes.

## Target design

### 1. Pull `ReportedRedirection` into `domain/` now (deviates from `request-controller.md`)

`request-controller.md` explicitly deferred the use case's success type to E3 ("guessed before readback
exists"). Readback is exactly what this MVP is for, so I'm proposing to do E3's domain-type half now, for
`logging` only:

```cpp
// domain/include/domain/redirection/reported_redirection.h
namespace domain {
struct ReportedRedirection
{
    std::optional<Logging> logging;   // kernel truth: absent only if the chain has no NFLOG rule
    bool operator==(const ReportedRedirection&) const = default;
};
}
```

```cpp
// domain/include/domain/use_case/interface_redirection_use_case.h
[[nodiscard]] virtual std::expected<ReportedRedirection, RedirectionProblem> execute(
    const RedirectionRequest& request) = 0;
```

This only changes the return type's payload (`void` → `ReportedRedirection`); `RedirectionProblem` and its
kinds (`KernelRejected`, `TableUnavailable`) already fit libiptc failures (`iptc_init` failing = table
unavailable; `iptc_commit`/`iptc_append_entry` failing = kernel rejected) with no new enum values needed.
`MockRedirectionUseCase` and its two existing tests change mechanically (`Return(ReportedRedirection{...})`
instead of `Return(std::expected<void,...>())`).

**Mirrors `contracts`:** `bellerophon/ngfw/redirection/reported.proto`'s `ReportedRedirection.logging`.
Kept hand-written like every other `domain/` type (no protobuf in `domain/`); a follow-up maps it to the
wire message once phase 2 exists, same relationship `request_parser` has with `DesiredRedirection` today.

### 2. New library `redirection_use_case/` (Ring 3, sibling of `controller/` and `request_parser/`)

Not `domain/` (which links nothing and must stay that way) and not `adapter_iptc/` (which must stay free
of every domain/contract concept per the placement doc). A new library that depends on `domain` (public,
for the request/problem/reported types) and `adapter_iptc`'s interfaces (private, injected — same rule
`request_parser` already follows for `contracts_cpp`/protobuf: "consumers link them, because this is a
static library, but cannot `#include` them").

```cpp
namespace redirection_use_case {

class NflogRedirectionUseCase final : public domain::IRedirectionUseCase
{
public:
    explicit NflogRedirectionUseCase(std::unique_ptr<IMangleAdapterFactory> adapterFactory);
    std::expected<domain::ReportedRedirection, domain::RedirectionProblem> execute(
        const domain::RedirectionRequest& request) override;

private:
    std::unique_ptr<IMangleAdapterFactory> m_adapterFactory;   // one MangleAdapter per id/chain
};

}
```

`IMangleAdapterFactory::forRedirection(int32_t id) -> std::unique_ptr<IMangleAdapter>` is the seam that
lets the use case be unit-tested with `MockMangleAdapter` without naming `MangleAdapter` (which owns a
real `IptcHelper`/`xtc_handle`). `adapter_iptc` already has `MockMangleAdapter`; only the factory
interface + its mock are new, plus the chain-naming convention that used to live in the caller.

### 3. Apply algorithm (idempotent, flush-and-rebuild — no new libiptc primitive needed)

```
execute(request):
    adapter = factory.forRedirection(request.id)           // chain name e.g. "bb_redir_<id>"
    try:
        adapter.ensureChain()                               // createChain(), EEXIST is not an error (fix #1)
        adapter.flush()                                      // idempotent: same result whether toggling on, off, or repeating
        if request.desired.logging has value:
            adapter.addNflogRule(request.desired.logging)    // no MarkParam for M0 — see open question 1
        adapter.commit()
    catch IptcException as e:
        return unexpected(RedirectionProblem{KernelRejected or TableUnavailable, e.what() filtered — see below})
    reported = adapter.readNflogRule()                       // new: iptc_first_rule/iptc_next_rule/iptc_get_target
    return ReportedRedirection{.logging = reported}
```

`flush()` before conditionally re-adding makes "remove" (`logging` goes absent) and "re-apply the same
state" the same code path as "add" — no separate delete call, no drift between add/remove logic. This is
the smallest correct thing, and it is what `MangleAdapter`'s existing tests already assume
(`flush()` then `add*Rule()`).

**Error detail must not forward libiptc's/kernel's string verbatim into `RedirectionProblem::detail`
unfiltered** — `iptc_strerror` text is safe (no payload content), but match the existing discipline in
`request_parser` (`describe()` builds details from known constants, never from what the caller sent) so a
future reply frame never leaks something it shouldn't.

### 4. `adapter_iptc` changes needed to support the above

- **`IIptcHelper`/`IptcHelper`: idempotent chain creation.** Either `ensureChain()` (checks
  `iptc_is_chain()` before calling `iptc_create_chain()`) as a new method, or make `createChain()` itself
  swallow "already exists" (checked via `errno == EEXIST` after a `false` return, not by string-matching
  `iptc_strerror`). Recommend the new `ensureChain()` name so `createChain()` keeps meaning "create, fail
  if present" for anything that legitimately wants that.
- **Read path.** Add to `IIptcHelper`: `firstRule()` / `nextRule(const ipt_entry*)`, mirroring
  `iptc_first_rule`/`iptc_next_rule` (no chain parameter, same as `createChain()`/`flushEntries()` — the
  chain is fixed at construction). Add to `IMangleAdapter`: `std::optional<NflogParam> readNflogRule()`,
  which walks the chain, finds the entry whose target name is `"NFLOG"` (same `xt_entry_target::u.user.name`
  every target already writes — no need for `iptc_get_target()`, which needs the handle; reading the
  buffer directly mirrors how `RuleBuilder`/`TemplateMatch` already write it) and decodes `xt_nflog_info`
  back into `NflogParam`. Validated against a real kernel above.
- **`NflogParam`/`NflogTarget`: add `range` and `threshold`.** `xt_nflog_info::len` + `XT_NFLOG_F_COPY_LEN`
  for range, `xt_nflog_info::threshold` directly. Both optional in the contract, so `NflogParam` gains
  `std::optional<uint32_t> range` / `std::optional<uint16_t> threshold` (or equivalent), and
  `readNflogRule()` reports them back as set only when the kernel shows a non-default value — actually
  simplest: track the flag/threshold==0 convention the kernel itself uses (see "open questions").
- **UTF-8-safe prefix handling**, not a byte-for-byte `strncpy`: truncate on a UTF-8 character boundary
  (never split a multi-byte sequence) when the contract-valid prefix does not fit in 63 bytes. Whatever
  the use case decides (truncate vs. reject — open question 4), `adapter_iptc` must not hand the kernel a
  cut codepoint.
- **Fix include guards**, add `namespace adapter_iptc { … }` (mechanical, but touches every file — see
  open question 6 on sequencing), and widen `.clang-tidy`'s `HeaderFilterRegex`.

### 5. `controller/` + `main.cpp` wiring (H-206) — only once the use case exists

Out of this MVP's critical path but worth sequencing correctly: `request-controller.md` already designs
`RequestController`/`ServeOutcome` in full. The one open item it leaves for later is `adapter_logger/`,
which needs `spdlog` — **not present in `conanfile.py` or in `vendor/conan/conan-cache.tgz`**, so
`adapter_logger` as specced cannot build here without a cache rebuild (needs network, which
`conan install --no-remote` in `make sandbox-build` forbids without first updating the image with
network access). Three ways forward, not decided here (open question 3):

1. Add `spdlog` to `conanfile.py`, rebuild the sandbox image with network, refresh `vendor/conan`. Matches
   the doc as written; costs an image rebuild before any of this compiles.
2. Build `adapter_logger::ILogger` on `libsystemd`'s `sd_journal_print`/`sd_journal_sendv` instead —
   already vendored, already linked by `adapter_socket`, gives structured journal fields natively (no
   custom formatting class needed), and is a thin syscall-style adapter consistent with
   `syscall-adapters.md`'s own pattern. Deviates from `request-controller.md`'s decision #4; would need
   that doc amended, not silently ignored.
3. Skip `adapter_logger` for this slice: `RequestController`/`main.cpp` stay unimplemented a bit longer;
   this MVP stops at `redirection_use_case` + its tests, proven through a unit test and the kernel
   integration tier below, and wiring lands in a follow-up once logging is settled.

## Milestones

| # | Deliverable | Tests |
|---|---|---|
| M0.1 | `domain::ReportedRedirection`; `IRedirectionUseCase::execute` returns it; update `MockRedirectionUseCase` + its 2 tests | `domain` unit tests (mechanical diff) |
| M0.2 | `adapter_iptc`: idempotent `ensureChain()`, NFLOG `range`/`threshold`, UTF-8-safe prefix truncation, include guards, `.clang-tidy` regex | unit tests per fix; repeat the `createChain()`-twice case that threw above, now green |
| M0.3 | `adapter_iptc`: read path (`IIptcHelper::firstRule/nextRule`, `IMangleAdapter::readNflogRule`) | unit tests against `MockIptcHelper` with a hand-built `ipt_entry` buffer (same style `mangle_adapter_test.cpp` already uses) |
| M0.4 | **New kernel integration tier**: a test binary that does what the exploration above did — real `MangleAdapter`, real kernel, isolated netns, no mocks — asserting add, idempotent re-apply, remove (flush), and read-back round-trip | gated (skips itself, not the build) when `unshare --user --map-root-user --net` is unavailable, so it degrades gracefully outside the sandbox; confirm inside the real Docker sandbox first (capability caveat above) |
| M0.5 | `redirection_use_case/` library: `NflogRedirectionUseCase`, `IMangleAdapterFactory` + mock | unit tests with `MockMangleAdapter`/`MockIMangleAdapterFactory`: add, remove (logging absent), kernel failure → `RedirectionProblem`, read-back mismatch surfaces as the real reported value (not an echo) |
| M0.6 | Decide + land one of the three logging options; if 2 or 3, note the `request-controller.md` amendment | n/a or new `adapter_logger` tests |

M0.1–M0.3 can be done in any order; M0.5 depends on M0.1–M0.3; M0.4 only needs M0.2–M0.3 and is valuable
proof independent of M0.5. M0.6 is independent of all of the above and only blocks `controller`/`main.cpp`.

## Open questions (need a decision before I start coding)

1. **Does M0's NFLOG rule need a `mark` match at all?** Nothing upstream of this chain exists yet to set a
   mark (DNAT/M1 is unbuilt), so an unconditional NFLOG in the per-id chain is the simplest correct thing
   for this slice and keeps `redirection_use_case` free of guessing a mark scheme M1 hasn't designed yet.
   Recommend: **new `IMangleAdapter::addNflogRule(NflogParam)` overload without `MarkParam`**, leaving the
   existing mark+NFLOG overload for M1. Alternative: reuse the existing overload with a fixed mark — rejected,
   because it invents plumbing the contract explicitly calls "internal or deferred."
2. **Chain naming and table.** Proposing `mangle`, chain `bb_redir_<id>` (matches the existing test fixture
   name style, `bb_mangle_redir_1`). Needs confirming against whatever M1 will want, since renaming a live
   chain later is a migration, not a refactor.
3. **Logging for this slice** — pick one of the three options in §5, or explicitly say "defer, no logging
   yet, only exit codes" until M0.6.
4. **Prefix longer than 63 bytes after UTF-8 truncation: truncate or reject?** The contract allows it
   (character count, not byte count); the kernel cannot hold it as given. Recommend **truncate at the
   UTF-8 boundary and let the read-back prove it** (the `reported` value will legitimately differ from
   `desired`, which is the README's whole point) rather than rejecting in `request_parser` (which would
   need a byte-budget rule `buf.validate` cannot express directly) or in the use case (which would need a
   new `RedirectionProblemKind` for something that is not really a kernel rejection).
5. **`range`/`threshold` read-back fidelity.** The kernel's "unset" for `threshold` is `0`, and for `range`
   is "`XT_NFLOG_F_COPY_LEN` flag clear" — both distinguishable from "set to zero/empty" the same way
   `Logging`'s optionals already do on the way in. Confirms the read path can always tell set-absent from
   set-to-default; flagging only because it means `readNflogRule()` needs the flag bit, not just the
   value.
6. **Namespacing `adapter_iptc`.** Worth doing for consistency and collision-safety before
   `redirection_use_case` starts depending on it, but it is a mechanical, wide-diff change across every
   file in the library and its tests with no behavior change. Propose doing it as its own commit, before
   M0.2, so the functional diffs that follow are reviewable on their own. Confirm you want it in scope now
   versus deferred to whenever `adapter_iptc` is extracted to its own repo (per the placement doc, that
   move is already a planned future event and would be a natural time to add the namespace instead).

## Out of scope (explicitly, so it is not assumed back in)

- DNAT, mark/conntrack wiring, hashlimit, `FilterAdapter`/`NatAdapter` — M1+.
- The phase-2 reply frame and `ProblemDetails` wire mapping for `RedirectionProblem` — E3/E4, needs the
  reply frame's own design first.
- `controller/`'s `ServeOutcome`/exit-code table and the systemd units — H-204/H-206/H-207, already
  designed in `request-controller.md`, just sequenced after this.
- Rewriting `-fpermissive` away — flagged as a finding, not folded into this slice's diff.

## Risks

- The kernel-integration tier (M0.4) needing `CAP_SYS_ADMIN`-equivalent inside the Docker sandbox is
  **unverified there** (only verified in this review's environment). If Ubuntu 24.04's AppArmor blocks
  unprivileged user namespaces in that container, the fallback is adding `CAP_SYS_ADMIN` to the sandbox's
  `cap_add` (broadens the sandbox's privilege, still fine since it is dev-only) or running that one test
  tier outside Docker.
- Pulling `ReportedRedirection` into `domain/` now is a deliberate deviation from `request-controller.md`'s
  sequencing (not its shape — the type it sketches for E3 is exactly this one). Flagging it rather than
  doing it silently, since that doc is a recorded decision.

## Rejected alternatives

- **Guess a mark value now so M0 and M1 share one code path.** Rejected: invents a scheme M1 hasn't
  designed, and `contracts/CLAUDE.md` is explicit that mark plumbing is internal and not to be front-run.
- **Put the use case inside `domain/`.** Rejected: `domain/` is header-only and links nothing
  (`domain/CMakeLists.txt`'s own `LEARN` comment); the use case must link `adapter_iptc`.
- **Put the use case inside `adapter_iptc/`.** Rejected: the placement doc is explicit that
  `adapter_iptc` never depends on any domain type, to keep it extractable to its own repo.
- **Silently truncate libiptc's error string into `RedirectionProblem::detail`.** Rejected for the same
  reason `request_parser` never forwards protobuf's own status message — `iptc_strerror` is safe today,
  but coupling a log line's shape to a vendored library's wording is the kind of thing that bites later.

## Suggested next step

If this matches what you want, I'd start at M0.1 → M0.3 (the `domain`/`adapter_iptc` groundwork, each its
own small commit, gated by the existing `format`/`tidy`/unit/ASan/Valgrind suite), then M0.4 as the proof,
then M0.5. M0.6 waits on your answer to open question 3.
