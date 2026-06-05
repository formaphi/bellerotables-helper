#ifndef MANGLE_ADAPTER_IMPL_H
#define MANGLE_ADAPTER_IMPL_H

#include "adapter_iptc/i_mangle_adapter.h"
#include "adapter_iptc/adapters/iptc_helper.h"

#include <string>

class MangleAdapter final : public IMangleAdapter {
public:
  MangleAdapter(const std::string &chain);
  ~MangleAdapter() = default;

  void createChain() override;
  void flush() override;
  void addConntrackRule(MarkParam mark, ConntrackParam conntrack) override;
  void addNflogRule(MarkParam mark, NflogParam nflog) override;
  void commit() override;

#ifdef UNIT_TEST
  void setHelper(const IptcHelper &helper);
#endif // UNIT_TEST

private:
  IptcHelper m_handle;
  std::string m_chain;
};

#endif /* MANGLE_ADAPTER_IMPL_H */
