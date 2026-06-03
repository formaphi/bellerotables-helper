#ifndef MANGLE_ADAPTER_H
#define MANGLE_ADAPTER_H

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

struct MarkParam {
    uint32_t mark;
};

struct NflogParam {
    std::string prefix;
    uint16_t group;
};

enum class ConntrackState { NEW, ESTABLISHED, RELATED, INVALID };

struct ConntrackParam {
    std::vector<ConntrackState> states;
};

class IptcException : public std::runtime_error {
public:
    explicit IptcException(const std::string& msg) : std::runtime_error(msg) {}
};

class IMangleAdapter {
public:
    virtual ~IMangleAdapter() = default;

    virtual void createChain() = 0;
    virtual void flush() = 0;
    virtual void addConntrackRule(MarkParam mark, ConntrackParam conntrack) = 0;
    virtual void addNflogRule(MarkParam mark, NflogParam nflog) = 0;
    virtual void commit() = 0;
};

#endif /* MANGLE_ADAPTER_H */
