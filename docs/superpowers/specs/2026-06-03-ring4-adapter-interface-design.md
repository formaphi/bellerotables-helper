---
title: Ring 4 Adapter Interface Design
date: 2026-06-03
tags: [ring4, adapter, interface, design]
---

# Ring 4 Adapter Interface Design

## Context

Ring 4 wraps libiptc complexity. Exposes clean interface consumed by Ring 3. Ring 3 combines adapter calls to implement business domains (monitoring, DoS, GeoIP, etc).

Ring 4 knows iptables vocabulary. Does NOT know business concepts.

## Decisions

| Decision | Choice |
|----------|--------|
| Abstraction level | Semantic methods (iptables vocabulary, not business domain) |
| Adapter per table | Yes — `MangleAdapter`, `FilterAdapter`, `NatAdapter` |
| Chain lifecycle | Adapter owns: create, flush, commit |
| Parameters | Grouped structs per match/target |
| Error handling | Custom exception |
| Handle ownership | RAII, auto re-init after commit |
| Internals | RuleBuilder + Match (Template Method) + ITarget (Strategy) |
| Method growth | Accept ~15–20 methods per adapter — reflects iptables table cardinality |

## Rationale

### One adapter per table

Each table has exclusive targets:

| Table | Exclusive targets |
|-------|------------------|
| mangle | MARK, CONNMARK |
| nat | DNAT, SNAT, MASQUERADE |
| filter | ACCEPT, DROP, NFLOG, NFQUEUE |

Interfaces are distinct — not the same interface duplicated. Each owns its `iptc_init("<table>")` handle.

### Semantic methods, no business sub-interfaces

Splitting into `IMangleMonitoring`, `IMangleMark`, etc. leaks business knowledge into Ring 4 — violates Adapter pattern. Methods reflect iptables operations, not domains.

### Re-init after commit

`iptc_commit` releases handle. Instead of single-use adapter, re-init automatically on next add* or createChain call. Transparent to caller.

## Interface Sketch (monitoring as example)

```cpp
// Parameter structs
struct MarkParam       { uint32_t mark; };
struct NflogParam      { std::string prefix; uint16_t group; };
struct ConntrackParam  { std::vector<ConntrackState> states; };
struct HashlimitParam  { uint32_t rate; uint32_t burst; std::string name; };

// Exceptions
class IptcException : public std::exception { ... };

// One adapter per table
class MangleAdapter {
public:
    void createChain(const std::string& name);
    void flush(const std::string& chain);
    void addMarkRule(const std::string& chain, /* proto/src/dst/dport params */, MarkParam);
    void addConntrackRule(const std::string& chain, MarkParam, ConntrackParam);
    void addNflogRule(const std::string& chain, MarkParam, NflogParam);
    void addHashlimitDropRule(const std::string& chain, MarkParam, HashlimitParam);
    void commit();
    // more methods added per feature (GeoIP, scheduling, etc.)

    // internals: iptc_handle*, RuleBuilder, re-init after commit
};

class FilterAdapter {
public:
    void createChain(const std::string& name);
    void flush(const std::string& chain);
    void addAcceptRule(const std::string& chain, MarkParam, /* dst/dport */);
    void addDropRule(const std::string& chain, /* src params */);
    void addNflogRule(const std::string& chain, NflogParam);
    void addNfqueueRule(const std::string& chain, MarkParam, uint16_t queue);
    void commit();
};

class NatAdapter {
public:
    void createChain(const std::string& name);
    void flush(const std::string& chain);
    void addDnatRule(const std::string& chain, MarkParam, /* proto/dport/dest/destPort */);
    void addSnatRule(const std::string& chain, /* src */, /* snatAddr */);
    void addMasqueradeRule(const std::string& chain, /* src */);
    void commit();
};
```

## Usage Example — Monitoring Feature

```cpp
// Ring 3 calls Ring 4 adapters directly

MangleAdapter mangle;
mangle.createChain("bb_mangle_redir_1");
mangle.addConntrackRule("bb_mangle_redir_1", {0x7d1}, {{RELATED, ESTABLISHED}});
mangle.addNflogRule("bb_mangle_redir_1", {0x7d1}, {"redir_1", 1});
mangle.commit();

FilterAdapter filter;
filter.createChain("bb_fw_redir_1");
filter.addNflogRule("bb_fw_redir_1", {"redir_1", 1});
filter.commit();
```

## Internal Mapping (Ring 4 only)

Each add* method uses `RuleBuilder` internally:

```
addNflogRule(chain, MarkParam, NflogParam)
  → RuleBuilder
      .addMatch(MarkMatch{mark})
      .setTarget(NflogTarget{prefix, group})
      .build()
  → iptc_append_entry(chain, entry, handle_)
```

Caller never touches buffer, offsets, or libiptc API.
