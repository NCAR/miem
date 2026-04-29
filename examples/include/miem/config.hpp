#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace miem {

enum class SourceMode { Offline, Online };
enum class SourceType { Anthropogenic, Fire, Biogenic, Dust, SeaSalt, Lightning };
enum class InventoryConvention { ECCAD, Descriptor };
enum class TemporalInterpolation { Linear, Nearest };
enum class VerticalInjection { Surface, Plume };

struct SpeciesMapping {
  std::string inventory_species_;
  std::string mechanism_species_;
  double      scaling_factor_ = 1.0;
};

struct SpeciesMap {
  std::string                  mechanism_;
  std::vector<SpeciesMapping>  mappings_;
};

struct DatasetDescriptor {
  std::string                        variable_prefix_;
  std::string                        flux_units_;
  double                             unit_conversion_factor_ = 1.0;
  std::string                        time_dimension_;
  std::string                        cell_dimension_;
  std::map<std::string, std::string> species_rename_;
};

struct SourceConfig {
  std::string                       name_;
  SourceMode                        mode_                   = SourceMode::Offline;
  SourceType                        type_                   = SourceType::Anthropogenic;
  std::string                       file_pattern_;
  InventoryConvention               convention_             = InventoryConvention::ECCAD;
  std::optional<DatasetDescriptor>  descriptor_;
  std::string                       provider_;
  SpeciesMap                        species_map_;
  TemporalInterpolation             temporal_interpolation_ = TemporalInterpolation::Linear;
  VerticalInjection                 vertical_injection_     = VerticalInjection::Surface;
  int                               category_               = 0;
  int                               hierarchy_              = 1;
  double                            scaling_factor_         = 1.0;
  std::string                       sector_;
};

struct MIEMConfig {
  std::string                version_;
  std::vector<SourceConfig>  sources_;
};

}  // namespace miem
