
#include <stdexcept>

class IptcException : public std::runtime_error {
public:
  explicit IptcException(const std::string &msg) : std::runtime_error(msg) {}
};
