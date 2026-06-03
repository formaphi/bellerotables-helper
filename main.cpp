#include <cerrno>
#include <cstdio>
#include <libiptc/libiptc.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include "MangleAdapter.h"
#include "LibiptcHandle.h"

static void dump_chain(const char* table, const char* chain) {
  auto* h = iptc_init(table);
  if (!h) {
    std::fprintf(stderr, "dump init failed: %s\n", iptc_strerror(errno));
    return;
  }
  const ipt_entry* e = iptc_first_rule(chain, h);
  int n = 0;
  while (e) {
    std::printf("  rule %d: proto=%d target_offset=%u next_offset=%u\n",
                ++n, e->ip.proto, e->target_offset, e->next_offset);
    std::size_t off = sizeof(ipt_entry);
    while (off < e->target_offset) {
      const auto* m = reinterpret_cast<const xt_entry_match*>(
          reinterpret_cast<const char*>(e) + off);
      std::printf("    match: %s\n", m->u.user.name);
      off += m->u.user.match_size;
    }
    const char* tgt = iptc_get_target(e, h);
    std::printf("    target: %s\n", (tgt && tgt[0]) ? tgt : "(standard)");
    e = iptc_next_rule(e, h);
  }
  if (n == 0) std::printf("  (empty)\n");
  iptc_free(h);
}

int main() {
  const std::string chain = "bb_mangle_redir_1";
  const uint32_t mark_hex = 2001 / 10;

  try {
    LibiptcHandle handle("mangle", chain);
    MangleAdapter mangle(handle, chain);

    mangle.createChain();

    mangle.addConntrackRule(
      {mark_hex},
      {{ConntrackState::RELATED, ConntrackState::ESTABLISHED}});

    mangle.addNflogRule(
      {mark_hex},
      {"d^1^1^allow", 1});

    mangle.commit();

    std::printf("committed to %s:\n", chain.c_str());
    dump_chain("mangle", chain.c_str());
  } catch (const IptcException& e) {
    std::fprintf(stderr, "iptc error: %s\n", e.what());
    return 1;
  }

  return 0;
}
