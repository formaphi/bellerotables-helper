#include "domain/mock/mock_redirection_use_case.h"

#include "domain/request/redirection_request.h"
#include "domain/use_case/interface_redirection_use_case.h"
#include "domain/use_case/redirection_problem.h"

#include <cstdint>
#include <expected>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>

using domain::IRedirectionUseCase;
using domain::MockRedirectionUseCase;
using domain::RedirectionProblem;
using domain::RedirectionProblemKind;
using domain::RedirectionRequest;
using testing::Return;

constexpr int32_t REDIRECTION_ID = 7;

/* LEARN: a plain mock, not StrictMock. StrictMock only changes calls to methods that have no
   EXPECT_CALL; the mock's single method has one here, so a wrong or extra call already fails.
   StrictMock/NiceMock also register the mock in gMock's never-freed reaction map, which Valgrind
   reports as still reachable (adapter_socket_tests uses plain mocks and frees every block).
*/

TEST(MockRedirectionUseCaseTest, ReturnsSuccessThroughTheInterface)
{
    auto mock = std::make_unique<MockRedirectionUseCase>();
    const RedirectionRequest request{.correlationId = "c", .id = REDIRECTION_ID};
    EXPECT_CALL(*mock, execute(request))
        .WillOnce(Return(std::expected<void, RedirectionProblem>()));
    const std::unique_ptr<IRedirectionUseCase> useCase = std::move(mock);

    const std::expected<void, RedirectionProblem> result = useCase->execute(request);

    EXPECT_TRUE(result.has_value());
}

TEST(MockRedirectionUseCaseTest, ReturnsTheProblemAsAnUnexpectedValue)
{
    auto mock = std::make_unique<MockRedirectionUseCase>();
    const RedirectionRequest request{.correlationId = "c", .id = REDIRECTION_ID};
    const RedirectionProblem problem{.kind = RedirectionProblemKind::TableUnavailable,
                                     .detail = "nat table missing"};
    EXPECT_CALL(*mock, execute(request)).WillOnce(Return(std::unexpected(problem)));
    const std::unique_ptr<IRedirectionUseCase> useCase = std::move(mock);

    const std::expected<void, RedirectionProblem> result = useCase->execute(request);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), problem);
}
