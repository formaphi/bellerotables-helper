#ifndef IPTC_BUILDER_H
#define IPTC_BUILDER_H

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_mark.h>
#include <linux/netfilter/xt_tcpudp.h>
#include <linux/netfilter/xt_conntrack.h>
#include <linux/netfilter/xt_hashlimit.h>
#include <linux/netfilter/nf_conntrack_common.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include "iptc_target.h"

class Match {
public:
  virtual ~Match() = default;

  std::size_t size() const {
    return XT_ALIGN(sizeof(xt_entry_match) + dataSize());
  }

  void write(unsigned char *ptr) const {
    auto *m = reinterpret_cast<xt_entry_match *>(ptr);
    m->u.user.match_size = size();
    std::strncpy(m->u.user.name, name(), sizeof(m->u.user.name) - 1);
    m->u.user.revision = revision();
    fillData(m->data);
  }

  virtual uint8_t protocol() const { return 0; }

protected:
  virtual const char *name() const = 0;
  virtual uint8_t revision() const = 0;
  virtual std::size_t dataSize() const = 0;
  virtual void fillData(unsigned char *data) const = 0;
};

// --- Matches ---

class MarkMatch : public Match {
  uint32_t mark_;
  uint32_t mask_;
public:
  MarkMatch(uint32_t mark, uint32_t mask = 0xffffffff)
    : mark_(mark), mask_(mask) {}

protected:
  const char *name() const override { return "mark"; }
  uint8_t revision() const override { return 1; }
  std::size_t dataSize() const override { return sizeof(xt_mark_mtinfo1); }

  void fillData(unsigned char *data) const override {
    auto *info = reinterpret_cast<xt_mark_mtinfo1 *>(data);
    info->mark = mark_;
    info->mask = mask_;
  }
};

class TcpMatch : public Match {
  uint16_t dport_min_;
  uint16_t dport_max_;
public:
  TcpMatch(uint16_t dport)
    : dport_min_(dport), dport_max_(dport) {}

  TcpMatch(uint16_t dport_min, uint16_t dport_max)
    : dport_min_(dport_min), dport_max_(dport_max) {}

  uint8_t protocol() const override { return IPPROTO_TCP; }

protected:
  const char *name() const override { return "tcp"; }
  uint8_t revision() const override { return 0; }
  std::size_t dataSize() const override { return sizeof(xt_tcp); }

  void fillData(unsigned char *data) const override {
    auto *info = reinterpret_cast<xt_tcp *>(data);
    info->spts[0] = 0;
    info->spts[1] = 65535;
    info->dpts[0] = dport_min_;
    info->dpts[1] = dport_max_;
  }
};

class UdpMatch : public Match {
  uint16_t dport_min_;
  uint16_t dport_max_;
public:
  UdpMatch(uint16_t dport)
    : dport_min_(dport), dport_max_(dport) {}

  UdpMatch(uint16_t dport_min, uint16_t dport_max)
    : dport_min_(dport_min), dport_max_(dport_max) {}

  uint8_t protocol() const override { return IPPROTO_UDP; }

protected:
  const char *name() const override { return "udp"; }
  uint8_t revision() const override { return 0; }
  std::size_t dataSize() const override { return sizeof(xt_udp); }

  void fillData(unsigned char *data) const override {
    auto *info = reinterpret_cast<xt_udp *>(data);
    info->spts[0] = 0;
    info->spts[1] = 65535;
    info->dpts[0] = dport_min_;
    info->dpts[1] = dport_max_;
  }
};

class ConntrackMatch : public Match {
  uint16_t match_flags_;
  uint16_t state_mask_;
public:
  ConntrackMatch(uint16_t match_flags, uint16_t state_mask)
    : match_flags_(match_flags), state_mask_(state_mask) {}

  static ConntrackMatch newState() {
    return ConntrackMatch(
      XT_CONNTRACK_STATE | XT_CONNTRACK_DIRECTION,
      NF_CT_STATE_BIT(IP_CT_NEW)
    );
  }

protected:
  const char *name() const override { return "conntrack"; }
  uint8_t revision() const override { return 3; }
  std::size_t dataSize() const override { return sizeof(xt_conntrack_mtinfo3); }

  void fillData(unsigned char *data) const override {
    auto *info = reinterpret_cast<xt_conntrack_mtinfo3 *>(data);
    info->match_flags = match_flags_;
    info->state_mask = state_mask_;
  }
};

class HashlimitMatch : public Match {
  char name_[IFNAMSIZ];
  uint32_t avg_;
  uint32_t burst_;
  uint32_t expire_;
  uint32_t gc_interval_;
  uint8_t srcmask_;
  uint8_t dstmask_;
  uint32_t mode_;
public:
  HashlimitMatch(const char *hl_name, uint32_t avg, uint32_t burst,
                 uint32_t expire, uint32_t gc_interval = 1000,
                 uint8_t srcmask = 32, uint8_t dstmask = 32,
                 uint32_t mode = XT_HASHLIMIT_HASH_SIP | XT_HASHLIMIT_HASH_DIP | XT_HASHLIMIT_HASH_DPT)
    : avg_(avg), burst_(burst), expire_(expire),
      gc_interval_(gc_interval), srcmask_(srcmask), dstmask_(dstmask), mode_(mode) {
    std::memset(name_, 0, sizeof(name_));
    std::strncpy(name_, hl_name, sizeof(name_) - 1);
  }

protected:
  const char *name() const override { return "hashlimit"; }
  uint8_t revision() const override { return 1; }
  std::size_t dataSize() const override { return sizeof(xt_hashlimit_mtinfo1); }

  void fillData(unsigned char *data) const override {
    auto *info = reinterpret_cast<xt_hashlimit_mtinfo1 *>(data);
    std::strncpy(info->name, name_, sizeof(info->name) - 1);
    info->cfg.mode = mode_;
    info->cfg.avg = avg_;
    info->cfg.burst = burst_;
    info->cfg.expire = expire_;
    info->cfg.gc_interval = gc_interval_;
    info->cfg.srcmask = srcmask_;
    info->cfg.dstmask = dstmask_;
  }
};

// --- Builder ---

class RuleBuilder {
  std::vector<const Match *> matches_;
  const ITarget *target_ = nullptr;
public:
  RuleBuilder &addMatch(const Match *m) {
    matches_.push_back(m);
    return *this;
  }

  RuleBuilder &setTarget(const ITarget *t) {
    target_ = t;
    return *this;
  }

  ipt_entry *build() const {
    std::size_t matches_size = 0;
    for (const auto *m : matches_)
      matches_size += m->size();

    auto target_offset = sizeof(ipt_entry) + matches_size;
    auto total = target_offset + target_->size();

    auto *entry = static_cast<ipt_entry *>(std::calloc(1, total));
    entry->target_offset = target_offset;
    entry->next_offset = total;
    entry->ip.proto = resolveProtocol();

    auto *ptr = entry->elems;
    for (const auto *m : matches_) {
      m->write(ptr);
      ptr += m->size();
    }

    target_->write(ptr);
    return entry;
  }

private:
  uint8_t resolveProtocol() const {
    for (const auto *m : matches_) {
      auto proto = m->protocol();
      if (proto != 0)
        return proto;
    }
    return 0;
  }
};

#endif
