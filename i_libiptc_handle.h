#ifndef I_LIBIPTC_HANDLE_H
#define I_LIBIPTC_HANDLE_H

#include <string>

struct ipt_entry;

class ILibiptcHandle {
public:
    virtual ~ILibiptcHandle() = default;

    virtual void createChain() = 0;
    virtual void flushEntries() = 0;
    virtual void appendEntry(const ipt_entry* entry) = 0;
    virtual void commit() = 0;
};

#endif /* I_LIBIPTC_HANDLE_H */
