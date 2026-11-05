#ifndef ADAPTER_SOCKET_TEST_PARAMETERIZED_CASE_H
#define ADAPTER_SOCKET_TEST_PARAMETERIZED_CASE_H

#include "adapter_socket/file_descriptor/readiness.h"

#include <concepts>
#include <gtest/gtest.h>
#include <ostream>
#include <string>
#include <system_error>

struct ErrnoCase
{
    std::errc error;
    const char* name;
};

struct ReadinessCase
{
    adapter_socket::Readiness readiness;
    const char* name;
};

template <typename Case>
concept NamedCase = requires(const Case& testCase) {
    { testCase.name } -> std::convertible_to<const char*>;
};

template <NamedCase Case>
std::string caseName(const ::testing::TestParamInfo<Case>& info)
{
    return info.param.name;
}

/* LEARN: gtest prints every parameter (test listing, XML report). Without a PrintTo it dumps the
   raw bytes of the struct, padding included, and Valgrind reports the uninitialised padding.
   gtest calls PrintTo unqualified, so argument-dependent lookup finds this overload for the case
   structs (global namespace), and being constrained it wins over gtest's generic fallback.
*/
template <NamedCase Case>
void PrintTo(const Case& testCase, std::ostream* stream)
{
    *stream << testCase.name;
}

#endif /* ADAPTER_SOCKET_TEST_PARAMETERIZED_CASE_H */
