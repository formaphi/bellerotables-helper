#ifndef IPTC_HELPER_H
#define IPTC_HELPER_H

#include "adapter_iptc/i_iptc_helper.h"

#include <string>
#include <vector>

struct xtc_handle;

/* LEARN: the only class that calls libiptc. It owns one xtc_handle (a snapshot of the table),
   opened lazily. iptc_commit() does not release the snapshot, so commit() frees it, and the next
   call opens a new one that reflects the kernel after the commit. Not copyable: two owners would
   free the same handle.
*/
class IptcHelper final : public IIptcHelper
{
public:
    IptcHelper(const std::string& table, const std::string& chain);
    ~IptcHelper() override;

    IptcHelper(const IptcHelper&) = delete;
    IptcHelper& operator=(const IptcHelper&) = delete;
    IptcHelper(IptcHelper&&) = delete;
    IptcHelper& operator=(IptcHelper&&) = delete;

    void createChain() override;
    void flushEntries() override;
    void appendEntry(const ipt_entry* entry) override;
    void commit() override;
    bool chainExists() override;
    std::vector<IptEntryPtr> listEntries() override;

private:
    void ensureOpen();
    void release();

    std::string m_table;
    std::string m_chain;
    xtc_handle* m_handle = nullptr;
};

#endif
