#include "request_parser/mock/mock_request_parser.h"

#include "domain/request/redirection_request.h"
#include "request_parser/parser/interface_request_parser.h"
#include "request_parser/parser/parse_problem.h"

#include <cstdint>
#include <expected>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <string_view>

using request_parser::IRequestParser;
using request_parser::MockRequestParser;
using request_parser::ParseProblem;
using request_parser::ParseProblemKind;
using testing::Return;

constexpr std::string_view PAYLOAD = R"({"correlation_id":"c","id":7,"desired":{}})";
constexpr int32_t REDIRECTION_ID = 7;

/* LEARN: a plain mock, not StrictMock. StrictMock only changes calls to methods that have no
   EXPECT_CALL; the mock's single method has one here, so a wrong or extra call already fails.
   StrictMock/NiceMock also register the mock in gMock's never-freed reaction map, which Valgrind
   reports as still reachable (adapter_socket_tests uses plain mocks and frees every block).
*/

TEST(MockRequestParserTest, ReturnsTheRequestThroughTheInterface)
{
    auto mock = std::make_unique<MockRequestParser>();
    const domain::RedirectionRequest request{.correlationId = "c", .id = REDIRECTION_ID};
    EXPECT_CALL(*mock, parse(PAYLOAD)).WillOnce(Return(request));
    const std::unique_ptr<IRequestParser> parser = std::move(mock);

    const std::expected<domain::RedirectionRequest, ParseProblem> result = parser->parse(PAYLOAD);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, request);
}

TEST(MockRequestParserTest, ReturnsTheProblemAsAnUnexpectedValue)
{
    auto mock = std::make_unique<MockRequestParser>();
    const ParseProblem problem{.kind = ParseProblemKind::MalformedJson, .detail = "not JSON"};
    EXPECT_CALL(*mock, parse(PAYLOAD)).WillOnce(Return(std::unexpected(problem)));
    const std::unique_ptr<IRequestParser> parser = std::move(mock);

    const std::expected<domain::RedirectionRequest, ParseProblem> result = parser->parse(PAYLOAD);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), problem);
}
