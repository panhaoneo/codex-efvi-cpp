#pragma once

#include <stdexcept>
#include <string>

namespace efvi {

class ViException : public std::runtime_error {
public:
  ViException(int code, const std::string& message)
      : std::runtime_error(message), code_(code) {}

  int code() const { return code_; }

private:
  int code_;
};

}  // namespace efvi
