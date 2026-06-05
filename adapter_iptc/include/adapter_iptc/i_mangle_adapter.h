#ifndef MANGLE_ADAPTER_H
#define MANGLE_ADAPTER_H

#include "adapter_iptc/dtos/conntrack.h"
#include "adapter_iptc/dtos/nflog.h"

class IMangleAdapter {
public:
  virtual ~IMangleAdapter() = default;

  virtual void createChain() = 0;
  virtual void flush() = 0;
  virtual void addConntrackRule(MarkParam mark, ConntrackParam conntrack) = 0;
  virtual void addNflogRule(MarkParam mark, NflogParam nflog) = 0;
  virtual void commit() = 0;
};

#endif /* MANGLE_ADAPTER_H */
