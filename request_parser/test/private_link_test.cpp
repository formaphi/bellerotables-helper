#include "parameterized_case.h"

#include <gtest/gtest.h>
#include <vector>

/* LEARN: this test binary is a consumer of request_parser, so its include path is exactly what
   any consumer gets. contracts_cpp, nlohmann and protobuf are linked PRIVATE: none of their
   headers may be reachable. domain is PUBLIC (IRequestParser returns domain types), so its headers
   must be. __has_include only works in the preprocessor; each answer becomes a constant here so a
   leak fails a named test instead of the build. The nlohmann and protobuf cases hold because the
   sandbox gets both only from Conan; a system copy (apt) would make them visible to everyone.
*/
#if __has_include(<bellerophon/ngfw/redirection/desired.pb.h>)
constexpr bool GENERATED_MESSAGES_VISIBLE = true;
#else
constexpr bool GENERATED_MESSAGES_VISIBLE = false;
#endif

#if __has_include(<bellerophon/ngfw/redirection/desired.pb.validate.h>)
constexpr bool GENERATED_VALIDATORS_VISIBLE = true;
#else
constexpr bool GENERATED_VALIDATORS_VISIBLE = false;
#endif

#if __has_include(<bvalidate/runtime.h>)
constexpr bool VALIDATOR_RUNTIME_VISIBLE = true;
#else
constexpr bool VALIDATOR_RUNTIME_VISIBLE = false;
#endif

#if __has_include(<nlohmann/json.hpp>)
constexpr bool NLOHMANN_VISIBLE = true;
#else
constexpr bool NLOHMANN_VISIBLE = false;
#endif

#if __has_include(<google/protobuf/message.h>)
constexpr bool PROTOBUF_VISIBLE = true;
#else
constexpr bool PROTOBUF_VISIBLE = false;
#endif

#if __has_include("request_parser/parser/interface_request_parser.h")
constexpr bool PARSER_INTERFACE_VISIBLE = true;
#else
constexpr bool PARSER_INTERFACE_VISIBLE = false;
#endif

#if __has_include("domain/request/redirection_request.h")
constexpr bool DOMAIN_REQUEST_VISIBLE = true;
#else
constexpr bool DOMAIN_REQUEST_VISIBLE = false;
#endif

struct HeaderVisibilityCase
{
    bool visible;
    bool expectedVisible;
    const char* name;
};

const std::vector<HeaderVisibilityCase> HEADER_VISIBILITY{
    {GENERATED_MESSAGES_VISIBLE, false, "GeneratedMessagesArePrivate"},
    {GENERATED_VALIDATORS_VISIBLE, false, "GeneratedValidatorsArePrivate"},
    {VALIDATOR_RUNTIME_VISIBLE, false, "ValidatorRuntimeIsPrivate"},
    {NLOHMANN_VISIBLE, false, "NlohmannIsPrivate"},
    {PROTOBUF_VISIBLE, false, "ProtobufIsPrivate"},
    {PARSER_INTERFACE_VISIBLE, true, "ParserInterfaceIsPublic"},
    {DOMAIN_REQUEST_VISIBLE, true, "DomainTypesArePublic"},
};

class PrivateLinkTest : public ::testing::TestWithParam<HeaderVisibilityCase>
{
};

TEST_P(PrivateLinkTest, HeaderVisibilityMatchesTheLinkScope)
{
    EXPECT_EQ(GetParam().visible, GetParam().expectedVisible);
}

INSTANTIATE_TEST_SUITE_P(Header, PrivateLinkTest, ::testing::ValuesIn(HEADER_VISIBILITY),
                         caseName<HeaderVisibilityCase>);
