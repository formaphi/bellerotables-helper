#ifndef CONTRACTS_CPP_TEST_VIOLATION_PRINTER_H
#define CONTRACTS_CPP_TEST_VIOLATION_PRINTER_H

#include <bvalidate/runtime.h>
#include <ostream>

namespace bvalidate {

inline void PrintTo(const Violation& violation, std::ostream* stream)
{
    *stream << "{" << violation.fieldPath << ", " << violation.ruleId << ", \"" << violation.message
            << "\"}";
}

} // namespace bvalidate

#endif // CONTRACTS_CPP_TEST_VIOLATION_PRINTER_H
