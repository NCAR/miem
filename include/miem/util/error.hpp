#pragma once

#include <stdexcept>
#include <string>

namespace miem {

class MIEMError : public std::runtime_error {
 public:
  MIEMError(const std::string& category, const std::string& message)
      : std::runtime_error(message), category_(category) {}

  const std::string& Category() const { return category_; }

 private:
  std::string category_;
};

class ConfigError : public MIEMError {
 public:
  explicit ConfigError(const std::string& message)
      : MIEMError("Configuration", message) {}
};

class IOError : public MIEMError {
 public:
  explicit IOError(const std::string& message)
      : MIEMError("IO", message) {}
};

class SpeciesError : public MIEMError {
 public:
  explicit SpeciesError(const std::string& message)
      : MIEMError("Species", message) {}
};

class ValidationError : public MIEMError {
 public:
  explicit ValidationError(const std::string& message)
      : MIEMError("Validation", message) {}
};

}  // namespace miem
