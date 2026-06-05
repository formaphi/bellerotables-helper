#ifndef I_IPTC_HELPER_H
#define I_IPTC_HELPER_H

struct ipt_entry;

class IIptcHelper {
public:
  virtual ~IIptcHelper() = default;
  virtual void createChain() = 0;
  virtual void flushEntries() = 0;
  virtual void appendEntry(const ipt_entry *entry) = 0;
  virtual void commit() = 0;
};

#endif
