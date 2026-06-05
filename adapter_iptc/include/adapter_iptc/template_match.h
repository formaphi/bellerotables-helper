#ifndef TEMPLATE_MATCH_H
#define TEMPLATE_MATCH_H

#include <cstdint>
#include <cstdlib>

class TemplateMatch {
public:
  virtual ~TemplateMatch() = default;

  std::size_t size() const;
  void write(unsigned char *ptr) const;
  virtual uint8_t protocol() const;

protected:
  virtual const char *name() const = 0;
  virtual uint8_t revision() const = 0;
  virtual std::size_t dataSize() const = 0;
  virtual void fillData(unsigned char *data) const = 0;
};

#endif
