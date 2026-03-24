#include "miem/source_factory.hpp"

#include "miem/source_offline.hpp"
#include "miem/util/error.hpp"

namespace miem {

std::unique_ptr<EmissionSource> SourceFactory::Create(
    const SourceConfig& config) {
  // All v1 source types (anthropogenic, fire, biogenic) use offline files.
  // Future source types (dust, sea_salt, online_biogenic) would get
  // additional branches here.
  if (config.type == "anthropogenic" || config.type == "fire" ||
      config.type == "biogenic") {
    return std::make_unique<OfflineEmissionSource>(config);
  }

  throw ConfigError("Unknown emission source type: '" + config.type +
                    "'. Supported types: anthropogenic, fire, biogenic");
}

}  // namespace miem
