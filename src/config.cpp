#include "miem/config.hpp"

#include <yaml-cpp/yaml.h>

#include "miem/util/error.hpp"

namespace miem {

MIEMConfig MIEMConfig::FromYAML(const std::string& yaml_path) {
  YAML::Node root;
  try {
    root = YAML::LoadFile(yaml_path);
  } catch (const YAML::Exception& e) {
    throw ConfigError("Failed to load MIEM config: " + yaml_path +
                      " — " + e.what());
  }

  auto miem_node = root["miem"];
  if (!miem_node) {
    throw ConfigError("Missing 'miem' key in: " + yaml_path);
  }

  MIEMConfig config;
  config.version = miem_node["version"]
      ? miem_node["version"].as<std::string>()
      : "1.0";

  auto sources_node = miem_node["sources"];
  if (!sources_node || !sources_node.IsSequence()) {
    throw ConfigError("Missing or invalid 'sources' in: " + yaml_path);
  }

  for (const auto& src : sources_node) {
    SourceConfig sc;
    sc.name = src["name"].as<std::string>();
    sc.type = src["type"] ? src["type"].as<std::string>() : "offline";
    sc.file_pattern = src["file_pattern"].as<std::string>();
    sc.species_map_path = src["species_map"].as<std::string>();

    if (src["temporal_interpolation"]) {
      sc.temporal_interpolation =
          ParseInterpolationMode(src["temporal_interpolation"].as<std::string>());
    }

    if (src["vertical_injection"]) {
      sc.vertical_injection = src["vertical_injection"].as<std::string>();
    }

    if (src["descriptor"]) {
      sc.descriptor_path = src["descriptor"].as<std::string>();
    }

    config.sources.push_back(std::move(sc));
  }

  return config;
}

}  // namespace miem
