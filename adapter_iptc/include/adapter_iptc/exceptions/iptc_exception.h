#ifndef IPTC_EXCEPTION_H
#define IPTC_EXCEPTION_H

#include <stdexcept>
#include <string>

/* LEARN: libiptc reports failures as a false/NULL return plus errno. errorNumber() keeps that errno
   so a caller can tell "the table is not there" (iptc_init) from "the kernel refused the rule"
   (iptc_commit) without parsing what(). It is 0 when the failure had no errno.
*/
class IptcException : public std::runtime_error
{
public:
    explicit IptcException(const std::string& msg)
        : std::runtime_error(msg)
    {
    }

    IptcException(const std::string& msg, int errorNumber)
        : std::runtime_error(msg)
        , m_errorNumber(errorNumber)
    {
    }

    [[nodiscard]] int errorNumber() const noexcept
    {
        return m_errorNumber;
    }

private:
    int m_errorNumber = 0;
};

#endif
