#pragma once

#include "request_parser/parser/interface_request_parser.h"

#include <gmock/gmock.h>

namespace request_parser {

class MockRequestParser : public IRequestParser
{
public:
    MOCK_METHOD((std::expected<domain::RedirectionRequest, ParseProblem>), parse,
                (std::string_view payload), (const, override));
};

} // namespace request_parser
