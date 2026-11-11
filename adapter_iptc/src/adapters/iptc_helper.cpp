#include "adapter_iptc/adapters/iptc_helper.h"

#include "adapter_iptc/exceptions/iptc_exception.h"
#include "adapter_iptc/ipt_entry_ptr.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <libiptc/libiptc.h>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace {

/* LEARN: errno is read first: iptc_strerror() maps it using the last libiptc call, and anything
   called in between may overwrite errno.
*/
[[noreturn]] void throwLastError(const std::string& call)
{
    const int errorNumber = errno;
    throw IptcException(call + ": " + iptc_strerror(errorNumber), errorNumber);
}

IptEntryPtr copyOf(const ipt_entry& entry)
{
    void* buffer = std::calloc(1, entry.next_offset);
    if (buffer == nullptr)
    {
        throw std::bad_alloc();
    }
    auto* rule = static_cast<ipt_entry*>(buffer);
    IptEntryPtr copy(rule, &std::free);
    std::memcpy(copy.get(), &entry, entry.next_offset);
    return copy;
}

} // namespace

IptcHelper::IptcHelper(std::string table, std::string chain)
    : m_table(std::move(table))
    , m_chain(std::move(chain))
{
}

IptcHelper::~IptcHelper()
{
    release();
}

void IptcHelper::ensureOpen()
{
    if (m_handle == nullptr)
    {
        m_handle = iptc_init(m_table.c_str());
        if (m_handle == nullptr)
        {
            throwLastError("iptc_init(" + m_table + ")");
        }
    }
}

void IptcHelper::release()
{
    if (m_handle != nullptr)
    {
        iptc_free(m_handle);
        m_handle = nullptr;
    }
}

void IptcHelper::createChain()
{
    ensureOpen();
    if (iptc_create_chain(m_chain.c_str(), m_handle) == 0)
    {
        throwLastError("iptc_create_chain(" + m_chain + ")");
    }
}

void IptcHelper::flushEntries()
{
    ensureOpen();
    if (iptc_flush_entries(m_chain.c_str(), m_handle) == 0)
    {
        throwLastError("iptc_flush_entries(" + m_chain + ")");
    }
}

void IptcHelper::appendEntry(const ipt_entry* entry)
{
    ensureOpen();
    if (iptc_append_entry(m_chain.c_str(), entry, m_handle) == 0)
    {
        throwLastError("iptc_append_entry(" + m_chain + ")");
    }
}

/* LEARN: the snapshot is released whether the commit succeeds or not. On failure the kernel kept
   its previous table and the pending changes are discarded, so the next call starts again from
   what the kernel really holds.
*/
void IptcHelper::commit()
{
    ensureOpen();
    const bool committed = iptc_commit(m_handle) != 0;
    if (!committed)
    {
        const int errorNumber = errno;
        const std::string reason = iptc_strerror(errorNumber);
        release();
        throw IptcException("iptc_commit(" + m_table + "): " + reason, errorNumber);
    }
    release();
}

bool IptcHelper::chainExists()
{
    ensureOpen();
    return iptc_is_chain(m_chain.c_str(), m_handle) != 0;
}

std::vector<IptEntryPtr> IptcHelper::listEntries()
{
    if (!chainExists())
    {
        throw IptcException("iptc_is_chain(" + m_chain + "): chain does not exist", ENOENT);
    }

    std::vector<IptEntryPtr> entries;
    for (const ipt_entry* entry = iptc_first_rule(m_chain.c_str(), m_handle); entry != nullptr;
         entry = iptc_next_rule(entry, m_handle))
    {
        entries.push_back(copyOf(*entry));
    }
    return entries;
}
