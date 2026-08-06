// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <vector>

namespace miem
{

  /// Immutable host-to-inventory cell selection.
  ///
  /// IDs are one-based inventory slots. Their order is the output order. An
  /// empty vector requests the backward-compatible full global grid.
  struct CellSelection
  {
    std::vector<int> global_cell_ids_;

    bool IsFullGrid() const
    {
      return global_cell_ids_.empty();
    }

    int SelectedCellCount(int global_n_cells) const
    {
      return IsFullGrid() ? global_n_cells : static_cast<int>(global_cell_ids_.size());
    }
  };

}  // namespace miem
