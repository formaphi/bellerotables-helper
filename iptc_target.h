#ifndef IPTC_TARGET_H
#define IPTC_TARGET_H

#include <cstdint>
#include <cstring>
#include <linux/netfilter.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_NFLOG.h>
#include <linux/netfilter_ipv4/ip_tables.h>

class ITarget {
public:
  virtual ~ITarget() = default;
  virtual std::size_t size() const = 0;
  virtual void write(unsigned char *ptr) const = 0;
};

class StandardTarget : public ITarget {
public:
  enum class Verdict { ACCEPT, DROP, RETURN };

  explicit StandardTarget(Verdict v) : verdict_(toKernel(v)) {}

  std::size_t size() const override {
    return XT_ALIGN(sizeof(xt_standard_target));
  }

  void write(unsigned char *ptr) const override {
    auto *st = reinterpret_cast<xt_standard_target *>(ptr);
    st->target.u.user.target_size = size();
    st->verdict = verdict_;
  }

private:
  int verdict_;

  static int toKernel(Verdict v) {
    switch (v) {
      case Verdict::ACCEPT: return -NF_ACCEPT - 1;
      case Verdict::DROP:   return -NF_DROP - 1;
      case Verdict::RETURN: return XT_RETURN;
    }
    return -NF_DROP - 1;
  }
};

class ExtensionTarget : public ITarget {
public:
  std::size_t size() const override {
    return XT_ALIGN(sizeof(xt_entry_target) + dataSize());
  }

  void write(unsigned char *ptr) const override {
    auto *t = reinterpret_cast<xt_entry_target *>(ptr);
    t->u.user.target_size = size();
    std::strncpy(t->u.user.name, name(), sizeof(t->u.user.name) - 1);
    fillData(t->data);
  }

protected:
  virtual const char *name() const = 0;
  virtual std::size_t dataSize() const = 0;
  virtual void fillData(unsigned char *data) const = 0;
};

class NflogTarget : public ExtensionTarget {
  uint16_t group_;
  char prefix_[64];
public:
  NflogTarget(uint16_t group, const char *prefix)
    : group_(group) {
    std::memset(prefix_, 0, sizeof(prefix_));
    std::strncpy(prefix_, prefix, sizeof(prefix_) - 1);
  }

protected:
  const char *name() const override { return "NFLOG"; }
  std::size_t dataSize() const override { return sizeof(xt_nflog_info); }

  void fillData(unsigned char *data) const override {
    auto *info = reinterpret_cast<xt_nflog_info *>(data);
    info->group = group_;
    std::strncpy(info->prefix, prefix_, sizeof(info->prefix) - 1);
  }
};

#endif
