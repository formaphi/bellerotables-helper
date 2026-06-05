#ifndef I_TARGET_STRATEGY_H
#define I_TARGET_STRATEGY_H

#include <cstdlib>

class ITargetStrategy {
public:
  virtual ~ITargetStrategy() = default;
  virtual std::size_t size() const = 0;
  virtual void write(unsigned char *ptr) const = 0;
};

#endif
