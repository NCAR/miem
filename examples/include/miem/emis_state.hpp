#pragma once

#include <map>
#include <string>

namespace miem {

struct FluxArray {
  double operator()(int /*cell*/, const std::string& /*species*/) const { return 0.0; }
};

struct EmisState {
  FluxArray                        surface_flux_;
  FluxArray                        tendency_;
  std::map<std::string, FluxArray> sector_fluxes_;
};

}  // namespace miem
