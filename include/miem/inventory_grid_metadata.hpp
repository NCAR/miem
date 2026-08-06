// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Immutable identity metadata copied from an exact-grid emissions inventory.
// Cell arrays are always in MIEM output order (full inventory order for an
// empty CellSelection, caller order otherwise).
#pragma once

#include <miem/util/types.hpp>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace miem
{

  enum class InventoryGridGeometry
  {
    Unknown,
    Planar,
    Spherical,
  };

  struct InventoryGridField
  {
    std::string units_;
    // Grid geometry is always retained in file precision, independently of
    // the configurable Real precision used for emissions fluxes.
    std::vector<double> values_;

    bool operator==(const InventoryGridField&) const = default;
  };

  struct InventoryGridMetadata
  {
    bool available_ = false;
    int global_n_cells_ = 0;
    std::vector<int> selected_global_cell_ids_;
    InventoryGridGeometry geometry_ = InventoryGridGeometry::Unknown;
    std::string on_a_sphere_;
    std::string is_periodic_;
    bool has_sphere_radius_ = false;
    double sphere_radius_ = 0.0;
    std::string fingerprint_algorithm_;
    std::string fingerprint_;
    std::string field_manifest_;
    std::string index_to_cell_id_units_;
    std::vector<std::int64_t> index_to_cell_id_;
    std::map<std::string, InventoryGridField> fields_;

    const InventoryGridField* FindField(const std::string& name) const
    {
      const auto it = fields_.find(name);
      return it == fields_.end() ? nullptr : &it->second;
    }

    bool IsExactGrid() const
    {
      const std::size_t selected_size = selected_global_cell_ids_.size();
      if (!available_ || global_n_cells_ < 1 || selected_size == 0 ||
          index_to_cell_id_.size() != selected_size ||
          fingerprint_algorithm_ != "chempas-mesh-sha256-v1" ||
          fingerprint_.size() != 64 || field_manifest_.empty() ||
          FindField("areaCell") == nullptr ||
          FindField("areaCell")->values_.size() != selected_size)
      {
        return false;
      }

      if (geometry_ == InventoryGridGeometry::Spherical)
      {
        return FindField("latCell") != nullptr && FindField("lonCell") != nullptr &&
               FindField("latCell")->values_.size() == selected_size &&
               FindField("lonCell")->values_.size() == selected_size;
      }
      if (geometry_ == InventoryGridGeometry::Planar)
      {
        for (const char* name : { "xCell", "yCell", "zCell" })
        {
          if (FindField(name) == nullptr || FindField(name)->values_.size() != selected_size)
          {
            return false;
          }
        }
        return true;
      }
      return false;
    }

    bool operator==(const InventoryGridMetadata&) const = default;
  };

}  // namespace miem
