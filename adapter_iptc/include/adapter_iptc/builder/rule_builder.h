#ifndef RULE_BUILDER_H
#define RULE_BUILDER_H

#include "adapter_iptc/i_iptc_builder.h"
#include "adapter_iptc/i_target_strategy.h"
#include "adapter_iptc/template_match.h"

#include <memory>
#include <vector>

class RuleBuilder final : public IBuilder
{
public:
    RuleBuilder& setMatch(const std::shared_ptr<TemplateMatch>& m);
    RuleBuilder& setTarget(const std::shared_ptr<ITargetStrategy>& t);
    std::shared_ptr<ipt_entry> build() const override;

private:
    std::vector<std::shared_ptr<TemplateMatch>> matches_;
    std::shared_ptr<ITargetStrategy> target_;
    uint8_t resolveProtocol() const;
};

#endif
