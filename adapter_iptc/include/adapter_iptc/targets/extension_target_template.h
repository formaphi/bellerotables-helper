#ifndef EXTENSION_TARGET_TEMPLATE_H
#define EXTENSION_TARGET_TEMPLATE_H

#include "adapter_iptc/i_target_strategy.h"

class ExtensionTargetTemplate : public ITargetStrategy {
public:
  std::size_t size() const override;
  void write(unsigned char *ptr) const override;

protected:
  virtual const char *name() const = 0;
  virtual std::size_t dataSize() const = 0;
  virtual void fillData(unsigned char *data) const = 0;
};

#endif
