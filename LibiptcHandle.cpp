#include "LibiptcHandle.h"
#include "mangle_adapter.h"

#include <cerrno>
#include <libiptc/libiptc.h>

LibiptcHandle::LibiptcHandle(const std::string& table, const std::string& chain)
    : m_table(table)
    , m_chain(chain)
    , m_handle(nullptr)
{
}

LibiptcHandle::~LibiptcHandle()
{
    if (m_handle) {
        iptc_free(static_cast<iptc_handle*>(m_handle));
    }
}

void LibiptcHandle::ensureOpen()
{
    if (!m_handle) {
        m_handle = iptc_init(m_table.c_str());
        if (!m_handle) {
            throw IptcException(iptc_strerror(errno));
        }
    }
}

void LibiptcHandle::createChain()
{
    ensureOpen();
    if (!iptc_create_chain(m_chain.c_str(), static_cast<iptc_handle*>(m_handle))) {
        throw IptcException(iptc_strerror(errno));
    }
}

void LibiptcHandle::flushEntries()
{
    ensureOpen();
    if (!iptc_flush_entries(m_chain.c_str(), static_cast<iptc_handle*>(m_handle))) {
        throw IptcException(iptc_strerror(errno));
    }
}

void LibiptcHandle::appendEntry(const ipt_entry* entry)
{
    ensureOpen();
    if (!iptc_append_entry(m_chain.c_str(), entry, static_cast<iptc_handle*>(m_handle))) {
        throw IptcException(iptc_strerror(errno));
    }
}

void LibiptcHandle::commit()
{
    ensureOpen();
    if (!iptc_commit(static_cast<iptc_handle*>(m_handle))) {
        throw IptcException(iptc_strerror(errno));
    }
    m_handle = nullptr;
}
