#pragma once

#include <cstring>
#include <exception>
#include <functional>

#include "miem/util/error.hpp"

// C-visible error struct. Uses MIEM_Error to avoid name collision with
// the C++ miem::MIEMError exception class.
extern "C" {

struct MIEM_Error {
  int code;
  char category[64];
  char message[256];
};

}  // extern "C"

namespace miem {
namespace c_api {

inline void ClearError(MIEM_Error* error) {
  if (error) {
    std::memset(error, 0, sizeof(MIEM_Error));
  }
}

inline void SetError(MIEM_Error* error, int code,
                     const char* category, const char* message) {
  if (error) {
    error->code = code;
    std::strncpy(error->category, category, sizeof(error->category) - 1);
    error->category[sizeof(error->category) - 1] = '\0';
    std::strncpy(error->message, message, sizeof(error->message) - 1);
    error->message[sizeof(error->message) - 1] = '\0';
  }
}

template <typename F>
void HandleErrors(MIEM_Error* error, F&& func) {
  ClearError(error);
  try {
    func();
  } catch (const miem::MIEMError& e) {
    // Catch MIEM's typed exceptions (ConfigError, IOError, etc.)
    // and preserve their category string
    SetError(error, 1, e.Category().c_str(), e.what());
  } catch (const std::exception& e) {
    SetError(error, 1, "Exception", e.what());
  } catch (...) {
    SetError(error, 2, "Unknown", "Unknown error occurred");
  }
}

}  // namespace c_api
}  // namespace miem
