#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <libiptc/libiptc.h>
#include "iptc_builder.h"

struct FreeDeleter { void operator()(void *p) { std::free(p); } };
using EntryPtr = std::unique_ptr<ipt_entry, FreeDeleter>;

struct HashlimitParams {
  std::string name;
  uint32_t rate;
  std::string unit;
  uint32_t burst;
  uint32_t expire_ms;
};

struct NflogParams {
  uint16_t group;
  std::string prefix;
};

struct MonitoringRule {
  std::string table;
  std::string chain;
  uint32_t mark;
  uint16_t dport;
  HashlimitParams hashlimit;
  NflogParams nflog;
};

uint32_t mark_to_hex(uint32_t db_mark) {
  return db_mark / 10;
}

uint32_t rate_to_avg(uint32_t rate, const std::string &unit) {
  uint32_t seconds = 1;
  if (unit == "minute") seconds = 60;
  else if (unit == "hour") seconds = 3600;
  return XT_HASHLIMIT_SCALE * seconds / rate;
}

std::string build_nflog_prefix(uint32_t redir_id, uint32_t log,
                               const std::string &action) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "d^%u^%u^%s", redir_id, log, action.c_str());
  return buf;
}

EntryPtr build_monitoring_rule(const MonitoringRule &rule) {
  MarkMatch mark(mark_to_hex(rule.mark));
  TcpMatch tcp(rule.dport);
  auto ct = ConntrackMatch::newState();
  HashlimitMatch hl(
    rule.hashlimit.name.c_str(),
    rate_to_avg(rule.hashlimit.rate, rule.hashlimit.unit),
    rule.hashlimit.burst,
    rule.hashlimit.expire_ms
  );
  NflogTarget nflog(rule.nflog.group, rule.nflog.prefix.c_str());

  return EntryPtr(RuleBuilder()
    .addMatch(&mark)
    .addMatch(&tcp)
    .addMatch(&ct)
    .addMatch(&hl)
    .setTarget(&nflog)
    .build());
}

void dump_chain(iptc_handle *reader, const char *chain_name) {
  auto *e = iptc_first_rule(chain_name, reader);
  auto rule_num = 0;
  while (e) {
    std::printf("\nrule %d: proto=%d target_offset=%u next_offset=%u\n",
                ++rule_num, e->ip.proto, e->target_offset, e->next_offset);

    auto off = sizeof(ipt_entry);
    while (off < e->target_offset) {
      auto *m = reinterpret_cast<const xt_entry_match *>(
          reinterpret_cast<const char *>(e) + off);
      std::printf("  match: %s (size=%u rev=%u)\n",
                  m->u.user.name, m->u.user.match_size, m->u.user.revision);
      off += m->u.user.match_size;
    }

    auto *tgt = iptc_get_target(e, reader);
    if (tgt && tgt[0] != '\0') {
      std::printf("  target: %s\n", tgt);
    } else {
      auto *st = reinterpret_cast<const xt_standard_target *>(
          reinterpret_cast<const char *>(e) + e->target_offset);
      const char *verdict_name = "UNKNOWN";
      if (st->verdict == -NF_ACCEPT - 1) verdict_name = "ACCEPT";
      else if (st->verdict == -NF_DROP - 1) verdict_name = "DROP";
      else if (st->verdict == XT_RETURN) verdict_name = "RETURN";
      std::printf("  target: %s (standard, verdict=%d)\n", verdict_name, st->verdict);
    }
    e = iptc_next_rule(e, reader);
  }
  if (rule_num == 0)
    std::printf("no rules in %s\n", chain_name);
}

int main() {
  MonitoringRule rule {
    "mangle",
    "bb_mangle_redir_1",
    2001,
    80,
    {"redir_1_allow", 12, "hour", 1, 60000},
    {1, build_nflog_prefix(1, 1, "allow")}
  };

  std::printf("--- input ---\n");
  std::printf("table=%s chain=%s mark=%u dport=%u\n",
              rule.table.c_str(), rule.chain.c_str(), rule.mark, rule.dport);
  std::printf("hashlimit: name=%s rate=%u/%s burst=%u expire=%u\n",
              rule.hashlimit.name.c_str(), rule.hashlimit.rate,
              rule.hashlimit.unit.c_str(), rule.hashlimit.burst,
              rule.hashlimit.expire_ms);
  std::printf("nflog: group=%u prefix=%s\n",
              rule.nflog.group, rule.nflog.prefix.c_str());

  std::printf("\n--- parsed ---\n");
  std::printf("mark_hex=0x%x\n", mark_to_hex(rule.mark));
  std::printf("hashlimit_avg=%u\n",
              rate_to_avg(rule.hashlimit.rate, rule.hashlimit.unit));

  auto monitoring_entry = build_monitoring_rule(rule);

  MarkMatch accept_mark(mark_to_hex(rule.mark));
  TcpMatch accept_tcp(rule.dport);
  StandardTarget accept_target(StandardTarget::Verdict::ACCEPT);
  EntryPtr accept_entry(RuleBuilder()
    .addMatch(&accept_mark)
    .addMatch(&accept_tcp)
    .setTarget(&accept_target)
    .build());

  auto *h = iptc_init(rule.table.c_str());
  if (!h) {
    std::fprintf(stderr, "iptc_init failed: %s\n", iptc_strerror(errno));
    return 1;
  }

  iptc_create_chain(rule.chain.c_str(), h);

  if (!iptc_append_entry(rule.chain.c_str(), monitoring_entry.get(), h)) {
    std::fprintf(stderr, "append monitoring failed: %s\n", iptc_strerror(errno));
    iptc_free(h);
    return 1;
  }

  if (!iptc_append_entry(rule.chain.c_str(), accept_entry.get(), h)) {
    std::fprintf(stderr, "append accept failed: %s\n", iptc_strerror(errno));
    iptc_free(h);
    return 1;
  }

  if (!iptc_commit(h)) {
    std::fprintf(stderr, "commit failed: %s\n", iptc_strerror(errno));
    iptc_free(h);
    return 1;
  }

  iptc_free(h);

  std::printf("\n--- verify ---\n");
  auto *reader = iptc_init(rule.table.c_str());
  if (!reader) {
    std::fprintf(stderr, "reader init failed: %s\n", iptc_strerror(errno));
    return 1;
  }
  dump_chain(reader, rule.chain.c_str());
  iptc_free(reader);

  return 0;
}
