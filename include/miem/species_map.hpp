// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <map>
#include <string>
#include <vector>

#include <miem/util/types.hpp>

namespace miem {

/// @brief A single inventory-species to mechanism-species mapping.
struct SpeciesMapping
{
  std::string inventory_name_;        ///< Name in the emission inventory.
  std::string mechanism_name_;        ///< Name in the chemical mechanism.
  Real        scaling_factor_ = 1.0;  ///< Mass-based per-mapping scaling.
};

/// @brief Programmatic inventory-to-mechanism species mapping.
///
/// Built incrementally with @ref AddMapping (typically by musica's
/// translator from the parsed configuration), then applied at runtime to
/// transform inventory flux into mechanism flux. There is no parsing
/// constructor; schema parsing lives in MechanismConfiguration.
class SpeciesMap
{
 public:
  SpeciesMap() = default;

  /// @brief Add a single mapping; rebuilds the cached mechanism index.
  void AddMapping(const std::string& inventory_name,
                  const std::string& mechanism_name,
                  Real               scaling_factor = 1.0);

  /// @brief Validate per-inventory-species scaling-factor sums.
  ///
  /// Each inventory species' scaling factors must sum to no more than
  /// \f$ 1.0 \f$ (plus a small tolerance) to avoid silent mass
  /// amplification.
  ///
  /// @throws MiemException (Species / SCALING_EXCEEDS_ONE) on the first
  ///         offending species.
  void Validate() const;

  /// @brief Transform inventory flux into mechanism flux.
  ///
  /// @param inventory_flux  Input, laid out as
  ///                        @c (n_inventory_species*n_cells).
  /// @param inventory_names Names matching the rows of @p inventory_flux.
  /// @param mechanism_flux  Output, laid out as
  ///                        @c (n_mechanism_species*n_cells); resized,
  ///                        zero-initialized, then accumulated.
  /// @param n_cells         Number of horizontal cells.
  ///
  /// @throws MiemException (Validation / CELL_COUNT_MISMATCH) when
  ///         @p inventory_flux is not sized
  ///         @c inventory_names.size()*n_cells.
  void Apply(const std::vector<Real>&        inventory_flux,
             const std::vector<std::string>& inventory_names,
             std::vector<Real>&              mechanism_flux,
             int                             n_cells) const;

  /// @brief Deduplicated list of mechanism-species names.
  std::vector<std::string> MechanismSpecies() const;
  /// @brief List of inventory-species names.
  std::vector<std::string> InventorySpecies() const;

  /// @brief The configured mappings.
  const std::vector<SpeciesMapping>& Mappings() const { return mappings_; }

  /// @brief Name of the chemical mechanism these mappings target.
  const std::string& MechanismName() const          { return mechanism_name_; }
  /// @brief Set the chemical-mechanism name.
  void SetMechanismName(const std::string& name)    { mechanism_name_ = name; }

 private:
  std::string                 mechanism_name_;
  std::vector<SpeciesMapping> mappings_;

  // Cached mechanism index, rebuilt by `AddMapping`.
  std::vector<std::string> cached_mechanism_species_;
  std::map<std::string, int> cached_mech_idx_;

  void RebuildCache();
};

}  // namespace miem
