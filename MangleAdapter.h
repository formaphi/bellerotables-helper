#ifndef MANGLE_ADAPTER_IMPL_H
#define MANGLE_ADAPTER_IMPL_H

#include "mangle_adapter.h"
#include "i_libiptc_handle.h"

#include <string>

class MangleAdapter final : public IMangleAdapter {
public:
    MangleAdapter(ILibiptcHandle& handle, const std::string& chain);
    ~MangleAdapter() = default;

    void createChain() override;
    void flush() override;
    void addConntrackRule(MarkParam mark, ConntrackParam conntrack) override;
    void addNflogRule(MarkParam mark, NflogParam nflog) override;
    void commit() override;

private:
    ILibiptcHandle& m_handle;
    std::string     m_chain;
};

#endif /* MANGLE_ADAPTER_IMPL_H */
