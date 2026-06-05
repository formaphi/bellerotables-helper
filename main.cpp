#include <cstdio>

#include "adapter_iptc/adapters/mangle_adapter.h"
#include "adapter_iptc/exceptions/iptc_exception.h"

int main() {
    const std::string chain = "bb_mangle_redir_1";
    const uint32_t mark = 2001 / 10;

    try {
        MangleAdapter mangle(chain);

        mangle.createChain();

        mangle.addConntrackRule(
            {mark},
            {ConntrackMatch::State::ESTABLISHED_RELATED});

        mangle.addNflogRule(
            {mark},
            {"d^1^1^allow", 1});

        mangle.commit();

        std::printf("committed chain: %s\n", chain.c_str());
    } catch (const IptcException& e) {
        std::fprintf(stderr, "iptc error: %s\n", e.what());
        return 1;
    }

    return 0;
}
