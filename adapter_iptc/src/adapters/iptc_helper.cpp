#include "adapter_iptc/adapters/iptc_helper.h"

#include "adapter_iptc/exceptions/iptc_exception.h"

#include <cerrno>
#include <libiptc/libiptc.h>

IptcHelper::IptcHelper(const std::string& table, const std::string& chain)
    : m_table(table)
    , m_chain(chain)
    , m_handle(nullptr)
{
}

IptcHelper::~IptcHelper()
{
    if (m_handle)
    {
        iptc_free(m_handle);
    }
}

void IptcHelper::ensureOpen()
{
    if (!m_handle)
    {
        m_handle = iptc_init(m_table.c_str());
        if (!m_handle)
        {
            throw IptcException(iptc_strerror(errno));
        }
    }
}

void IptcHelper::createChain()
{
    ensureOpen();
    if (!iptc_create_chain(m_chain.c_str(), m_handle))
    {
        throw IptcException(iptc_strerror(errno));
    }
}

void IptcHelper::flushEntries()
{
    ensureOpen();
    if (!iptc_flush_entries(m_chain.c_str(), m_handle))
    {
        throw IptcException(iptc_strerror(errno));
    }
}

void IptcHelper::appendEntry(const ipt_entry* entry)
{
    ensureOpen();
    if (!iptc_append_entry(m_chain.c_str(), entry, m_handle))
    {
        throw IptcException(iptc_strerror(errno));
    }
}

void IptcHelper::commit()
{
    ensureOpen();
    if (!iptc_commit(m_handle))
    {
        throw IptcException(iptc_strerror(errno));
    }
    m_handle = nullptr;
}
