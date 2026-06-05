#ifndef MOCK_TEMPLATE_MATCH_H
#define MOCK_TEMPLATE_MATCH_H

#include <gmock/gmock.h>

#include "adapter_iptc/template_match.h"

class MockTemplateMatch : public TemplateMatch {
public:
  MOCK_CONST_METHOD0(protocol, uint8_t());
  MOCK_CONST_METHOD0(name, const char *());
  MOCK_CONST_METHOD0(revision, uint8_t());
  MOCK_CONST_METHOD0(dataSize, std::size_t());
  MOCK_CONST_METHOD1(fillData, void(unsigned char *data));
};

#endif
