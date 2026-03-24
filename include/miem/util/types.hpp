#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace miem {

#ifdef MIEM_USE_DOUBLE
using Real = double;
#else
using Real = float;
#endif

using Index = std::size_t;

}  // namespace miem
