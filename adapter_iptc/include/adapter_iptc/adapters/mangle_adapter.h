#ifndef MANGLE_ADAPTER_IMPL_H
#define MANGLE_ADAPTER_IMPL_H

#include "adapter_iptc/i_mangle_adapter.h"
#include "adapter_iptc/i_iptc_helper.h"
#include "adapter_iptc/adapters/iptc_helper.h"

#include <memory>
#include <string>

class MangleAdapter final : public IMangleAdapter {
public:
  MangleAdapter(const std::string &chain);

  void createChain() override;
  void flush() override;
  void addConntrackRule(MarkParam mark, ConntrackParam conntrack) override;
  void addNflogRule(MarkParam mark, NflogParam nflog) override;
  void commit() override;

  void setHelper(std::shared_ptr<IIptcHelper> helper);

private:
  std::shared_ptr<IIptcHelper> m_helper;
};

#endif
