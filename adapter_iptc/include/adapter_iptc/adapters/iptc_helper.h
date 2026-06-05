#ifndef IPTC_HELPER_H
#define IPTC_HELPER_H

#include "adapter_iptc/i_iptc_helper.h"

#include <string>

struct xtc_handle;

class IptcHelper final : public IIptcHelper {
public:
  IptcHelper(const std::string &table, const std::string &chain);
  ~IptcHelper();

  void createChain() override;
  void flushEntries() override;
  void appendEntry(const ipt_entry *entry) override;
  void commit() override;

private:
  void ensureOpen();

  std::string m_table;
  std::string m_chain;
  xtc_handle *m_handle;
};

#endif
