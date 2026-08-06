// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <miem/util/types.hpp>

#include <map>
#include <string>
#include <vector>

namespace miem
{

  /// @brief Two-dimensional view backed by a flat @c std::vector<Real>.
  ///
  /// Rows are species, columns are cells. Stored in species-major
  /// (row-major) order, so @c &data()[species_idx*n_cells_] is the start of
  /// a row.
  class EmissionsArray
  {
   public:
    EmissionsArray() = default;
    EmissionsArray(int n_species, int n_cells)
        : n_species_(n_species),
          n_cells_(n_cells),
          data_(static_cast<std::size_t>(n_species) * n_cells, Real{ 0 })
    {
    }

    /// @brief Resize to @p n_species x @p n_cells and zero-fill.
    void Resize(int n_species, int n_cells)
    {
      n_species_ = n_species;
      n_cells_ = n_cells;
      data_.assign(static_cast<std::size_t>(n_species) * n_cells, Real{ 0 });
    }

    /// @brief Set the species names and rebuild the name-to-index map.
    void SetSpecies(const std::vector<std::string>& names)
    {
      species_names_ = names;
      species_index_.clear();
      for (int i = 0; i < static_cast<int>(names.size()); ++i)
      {
        species_index_[names[i]] = i;
      }
    }

    /// @brief Value at (@p cell, @p species) by name; zero if not present.
    Real operator()(int cell, const std::string& species) const
    {
      auto it = species_index_.find(species);
      if (it == species_index_.end() || cell < 0 || cell >= n_cells_)
      {
        return Real{ 0 };
      }
      return data_[static_cast<std::size_t>(it->second) * n_cells_ + cell];
    }

    /// @brief Mutable reference at (@p species_idx, @p cell).
    Real& At(int species_idx, int cell)
    {
      return data_[static_cast<std::size_t>(species_idx) * n_cells_ + cell];
    }

    /// @brief Value at (@p species_idx, @p cell).
    Real At(int species_idx, int cell) const
    {
      return data_[static_cast<std::size_t>(species_idx) * n_cells_ + cell];
    }

    int n_species() const
    {
      return n_species_;
    }  ///< Row count.
    int n_cells() const
    {
      return n_cells_;
    }  ///< Column count.
    Real* data()
    {
      return data_.data();
    }
    const Real* data() const
    {
      return data_.data();
    }
    std::size_t size() const
    {
      return data_.size();
    }
    const std::vector<Real>& raw() const
    {
      return data_;
    }
    std::vector<Real>& raw()
    {
      return data_;
    }
    const std::vector<std::string>& SpeciesNames() const
    {
      return species_names_;
    }

   private:
    int n_species_ = 0;
    int n_cells_ = 0;
    std::vector<Real> data_;
    std::vector<std::string> species_names_;
    std::map<std::string, int> species_index_;
  };

  /// @brief Per-cell, per-species emissions output returned by
  ///        @c Emissions::Run.
  ///
  /// Holds the surface flux and volumetric tendency arrays, plus optional
  /// per-sector diagnostic fluxes. All field names carry a trailing
  /// underscore.
  ///
  /// Memory layout:
  /// - @c surface_flux_ : species-major, @c [species_idx*n_cells_+cell_idx]
  /// - @c layer_flux_   : species + level major,
  ///   @c [species_idx*n_vert_levels_*n_cells_ + level_idx*n_cells_ + cell_idx]
  /// - @c tendency_     : species + level major,
  ///   @c [species_idx*n_vert_levels_*n_cells_ + level_idx*n_cells_ + cell_idx]
  ///
  /// The convenience accessor @c operator()(cell, species) resolves by
  /// species name via an index map maintained alongside the species list.
  /// Raw-pointer accessors are exposed for the zero-copy hand-off to the
  /// host model / MUSICA layer.
  struct EmissionsState
  {
    int n_species_ = 0;      ///< Number of emission species.
    int n_cells_ = 0;        ///< Number of horizontal cells.
    int n_vert_levels_ = 0;  ///< Number of vertical levels.

    std::vector<std::string> species_names_;  ///< Emission species names.

    /// @brief Maps emission species index to host chemistry species index
    ///        (-1 if not present). Populated by
    ///        @c Emissions::ResolveHostIndices.
    std::vector<int> emis_to_chem_idx_;

    /// @brief Per-species injection layer (0 = surface).
    /// Retained for legacy states that do not populate layer_flux_.
    std::vector<int> injection_layer_;

    EmissionsArray surface_flux_;  ///< (n_species, n_cells) [kg m-2 s-1].

    /// @brief Per-layer area flux, flat
    ///        (n_species * n_vert_levels * n_cells) [kg m-2 s-1].
    std::vector<Real> layer_flux_;
    bool has_layer_flux_ = false;

    /// @brief Volumetric tendency, flat
    ///        (n_species * n_vert_levels * n_cells) [kg kg-1 s-1].
    std::vector<Real> tendency_;

    /// @brief Optional diagnostic per-sector fluxes (same shape as
    ///        @c surface_flux_).
    std::map<std::string, EmissionsArray> sector_fluxes_;
    std::vector<std::string> sector_names_;  ///< Sector insertion order.

    EmissionsState() = default;
    EmissionsState(int n_species, int n_cells, int n_vert_levels)
    {
      Resize(n_species, n_cells, n_vert_levels);
    }

    /// @brief Resize all arrays to the given dimensions and zero-fill.
    void Resize(int n_species, int n_cells, int n_vert_levels);
    /// @brief Zero all flux and tendency arrays in place.
    void Zero();

    /// @brief True iff any per-sector diagnostic fluxes are present.
    bool HasSectors() const
    {
      return !sector_fluxes_.empty();
    }

    /// @brief Sector flux by name; returns nullptr if the sector is absent.
    const EmissionsArray* GetSectorFlux(const std::string& sector) const;

    /// @name Raw-pointer accessors for the zero-copy host/MUSICA hand-off.
    /// @{
    Real* SurfaceFluxData()
    {
      return surface_flux_.data();
    }
    const Real* SurfaceFluxData() const
    {
      return surface_flux_.data();
    }
    Real* LayerFluxData()
    {
      return layer_flux_.data();
    }
    const Real* LayerFluxData() const
    {
      return layer_flux_.data();
    }
    Real* TendencyData()
    {
      return tendency_.data();
    }
    const Real* TendencyData() const
    {
      return tendency_.data();
    }
    int* EmisToChemIdxData()
    {
      return emis_to_chem_idx_.data();
    }
    const int* EmisToChemIdxData() const
    {
      return emis_to_chem_idx_.data();
    }
    /// @}
  };

}  // namespace miem
