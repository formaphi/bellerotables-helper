#ifndef LIBIPTC_HANDLE_H
#define LIBIPTC_HANDLE_H

#include "i_libiptc_handle.h"

#include <string>

class LibiptcHandle final : public ILibiptcHandle {
public:
    LibiptcHandle(const std::string& table, const std::string& chain);
    ~LibiptcHandle();

    void createChain() override;
    void flushEntries() override;
    void appendEntry(const ipt_entry* entry) override;
    void commit() override;

private:
    void ensureOpen();

    std::string m_table;
    std::string m_chain;
    void*       m_handle;
};

#endif /* LIBIPTC_HANDLE_H */
