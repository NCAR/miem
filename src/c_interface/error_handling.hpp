#pragma once

#include <cstring>
#include <exception>
#include <functional>

extern "C" {

struct MIEMError {
  int code;
  char category[64];
  char message[256];
};

}  // extern "C"

namespace miem {
namespace c_api {

inline void ClearError(MIEMError* error) {
  if (error) {
    error->code = 0;
    error->category[0] = '\0';
    error->message[0] = '\0';
  }
}

inline void SetError(MIEMError* error, int code,
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
void HandleErrors(MIEMError* error, F&& func) {
  ClearError(error);
  try {
    func();
  } catch (const MIEMError& e) {
    // Already a C error struct — shouldn't happen but handle gracefully
    if (error) *error = e;
  } catch (const std::exception& e) {
    SetError(error, 1, "Exception", e.what());
  } catch (...) {
    SetError(error, 2, "Unknown", "Unknown error occurred");
  }
}

}  // namespace c_api
}  // namespace miem
