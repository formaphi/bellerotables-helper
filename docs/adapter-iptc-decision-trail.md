# adapter_iptc — Decision Trail

Rationale behind the `adapter_iptc` library. Reconstructed from the `feat/iptc-adapter`
branch history, the [Ring 4 Adapter Interface Design](superpowers/specs/2026-06-03-ring4-adapter-interface-design.md)
spec, and the final code.

## Timeline

| Commit | Step |
|--------|------|
| `3848f02` | libiptc PoC findings — buffer layout, standard vs extension targets, gotchas |
| `66ecf0a` | direct libiptc experiments |
| `9c4649e` | Docker sandbox for testing rule writes |
| `23dcc70` | first adaptee (`LibiptcHandle`) isolating the C API |
| `20f877d`, `936c3d5` | initial `MangleAdapter` + usage in `main` (branch `poc/ring4-adapters`) |
| `4046e0d`, `c029fad` | `iptables-legacy` built on AlmaLinux 8 to verify what libiptc writes |
| `01076b3` | `adapter_iptc` promoted to its own CMake module |
| `c9f5d8f` | base interfaces (`IMangleAdapter`, `IIptcBuilder`, `ITargetStrategy`) |
| `0bba438` → `2619588` | adapters, rule builder, monitoring DTOs, matches, targets, exception |
| `6ddc2e6` | helper abstraction rework — extracts `IIptcHelper` |
| `f6f802a` | mocks for every interface |
| `1fc1cae`, `8fba1ff`, `d1a98ee` | lib wired into `main`, ring docs, dead code removal |

The PoC came before the design: the interfaces were only drawn once the libiptc binary
buffer was understood in practice.

## Decisions and rationale

| Decision | Choice | Why |
|----------|--------|-----|
| Abstraction level | Semantic methods in iptables vocabulary | Per-feature sub-interfaces (`IMangleMonitoring`, `IMangleMark`) would leak business knowledge into Ring 4 and violate the Adapter pattern |
| One adapter per table | `MangleAdapter` (+ future `FilterAdapter`, `NatAdapter`) | Each table has exclusive targets — mangle: MARK/CONNMARK; nat: DNAT/SNAT/MASQUERADE; filter: ACCEPT/DROP/NFLOG/NFQUEUE. These are distinct interfaces, not one duplicated; each owns its `iptc_init("<table>")` handle |
| Chain lifecycle | Adapter owns create, flush, commit | Caller manages neither the handle nor the operation order |
| Parameters | Grouped structs per match/target (`MarkParam`, `NflogParam`, `ConntrackParam`, `HashlimitParam`) | Avoids long positional signatures |
| Error handling | `IptcException` | libiptc signals failure via return code + `errno`; translated to an exception in one place |
| Handle ownership | RAII + automatic re-init after commit | `iptc_commit` releases the handle; instead of a single-use adapter, it reopens on the next `add*`/`createChain`, transparently to the caller |
| Internals | `RuleBuilder` + `TemplateMatch` (Template Method) + `ITargetStrategy` (Strategy) | Caller never touches the buffer, offsets, or the libiptc API |
| Method growth | Accept ~15–20 methods per adapter | Reflects the real cardinality of the table's operations; splitting per feature would reintroduce the domain leak |

## Spec vs implementation divergences

Two changes introduced after the spec, in `6ddc2e6` and `f6f802a`:

**Chain in the constructor.** The spec passed the chain to every method
(`createChain(name)`, `addNflogRule(chain, ...)`). The implementation moves it to the
constructor — `MangleAdapter(const std::string &chain)` — and drops the parameter from the
methods. One adapter instance per redir rule.

**`IIptcHelper` as an abstract adaptee.** The spec held a raw `xtc_handle*` inside the
adapter. The implementation extracts `IptcHelper` (wrapping `iptc_init`, `createChain`,
`flushEntries`, `appendEntry`, `commit`) behind the `IIptcHelper` interface, injected via
`setHelper(std::shared_ptr<IIptcHelper>)`. This is what makes `MangleAdapter` testable
without NET_ADMIN or a kernel — it enabled the mocks in `f6f802a`.

**Not implemented.** `FilterAdapter` and `NatAdapter` still exist only in the spec.

## Related documents

- [Ring 4 Adapter Interface Design](superpowers/specs/2026-06-03-ring4-adapter-interface-design.md) — original spec
- `CLAUDE.md` / `README.md` — libiptc mechanics (buffer layout, 8-step flow, standard vs
  extension target) and platform gotchas (`ip4tc` instead of `iptc`, `-fpermissive`,
  `gc_interval > 0`, nftables vs legacy on AlmaLinux 8)
