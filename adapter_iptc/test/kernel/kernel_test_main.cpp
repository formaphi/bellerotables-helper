#include "network_namespace.h"

#include <gtest/gtest.h>
#include <string>

namespace {

std::string g_isolationFailure;

} // namespace

const std::string& isolationFailure()
{
    return g_isolationFailure;
}

int main(int argc, char** argv)
{
    g_isolationFailure = enterIsolatedNetworkNamespace();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
