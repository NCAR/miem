#include "miem/dataset_descriptor.hpp"

#include <yaml-cpp/yaml.h>

#include "miem/util/error.hpp"

namespace miem {

DatasetDescriptor DatasetDescriptor::Default() {
  return DatasetDescriptor{};
}

DatasetDescriptor DatasetDescriptor::FromYAML(const std::string& yaml_path) {
  YAML::Node root;
  try {
    root = YAML::LoadFile(yaml_path);
  } catch (const YAML::Exception& e) {
    throw ConfigError("Failed to load descriptor: " + yaml_path +
                      " — " + e.what());
  }

  auto desc_node = root["descriptor"];
  if (!desc_node) {
    throw ConfigError("Missing 'descriptor' key in: " + yaml_path);
  }

  DatasetDescriptor desc;

  if (desc_node["variable_prefix"]) {
    desc.variable_prefix = desc_node["variable_prefix"].as<std::string>();
  }
  if (desc_node["flux_units"]) {
    desc.flux_units = desc_node["flux_units"].as<std::string>();
  }
  if (desc_node["unit_conversion_factor"]) {
    desc.unit_conversion_factor = desc_node["unit_conversion_factor"].as<Real>();
  }
  if (desc_node["time_dimension"]) {
    desc.time_dimension = desc_node["time_dimension"].as<std::string>();
  }
  if (desc_node["cell_dimension"]) {
    desc.cell_dimension = desc_node["cell_dimension"].as<std::string>();
  }

  if (desc_node["species_rename"]) {
    for (const auto& pair : desc_node["species_rename"]) {
      desc.species_rename[pair.first.as<std::string>()] =
          pair.second.as<std::string>();
    }
  }

  return desc;
}

}  // namespace miem
