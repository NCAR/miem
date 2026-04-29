// SPDX-License-Identifier: Apache-2.0
//
// Worked MIEM API example.
//
// Constructs a miem::MIEMConfig programmatically and runs one emissions
// timestep. Mirrors how a non-musica caller (ad-hoc host integration,
// test harness, standalone tool) feeds MIEM. Musica's YAML-to-MIEMConfig
// path is shown in musica's examples, not here — by design MIEM never
// sees YAML.
//
// See docs/config-architecture.md §3 for struct ownership and §5 for
// the public API shape. See examples/README.md for a prose walkthrough
// and the open-design-questions list this example surfaces.
//
// NOTE: The miem/ headers included below are stubs that define the API
// surface and compile cleanly but contain no real implementation.
// Output values are placeholders. See the README for details.

#include <miem/config.hpp>            // MIEMConfig, SourceConfig, SpeciesMap,
                                      // DatasetDescriptor + enums
#include <miem/emissions_module.hpp>  // EmissionsModule
#include <miem/emis_state.hpp>        // EmisState

#include <iomanip>
#include <iostream>
#include <vector>

using namespace miem;

int main()
{
  // --------------------------------------------------------------
  // 1. Shared species maps.
  //
  //    Each SpeciesMap belongs to a mechanism. A mapping can split
  //    one inventory species across multiple mechanism species with
  //    scaling factors; mass-conservation rule says the per-inventory-
  //    species sum must be ≤ 1.0 (see architecture doc §7). Sums < 1.0
  //    mean mass is routed to species the mechanism does not track
  //    and silently dropped — the CAMS NMVOC split below demonstrates
  //    this (sum = 0.65).
  // --------------------------------------------------------------
  SpeciesMap mozart_t1_from_cams{ .mechanism_ = "MOZART-T1" };
  mozart_t1_from_cams.mappings_ = {
    { .inventory_species_ = "NOx",   .mechanism_species_ = "NO",      .scaling_factor_ = 0.9 },
    { .inventory_species_ = "NOx",   .mechanism_species_ = "NO2",     .scaling_factor_ = 0.1 },
    { .inventory_species_ = "SO2",   .mechanism_species_ = "SO2",     .scaling_factor_ = 1.0 },
    { .inventory_species_ = "CO",    .mechanism_species_ = "CO",      .scaling_factor_ = 1.0 },
    { .inventory_species_ = "NH3",   .mechanism_species_ = "NH3",     .scaling_factor_ = 1.0 },
    { .inventory_species_ = "BC",    .mechanism_species_ = "BC",      .scaling_factor_ = 1.0 },
    { .inventory_species_ = "OC",    .mechanism_species_ = "OC",      .scaling_factor_ = 1.0 },
    // Lumped NMVOC split. Sum = 0.65 < 1.0 — 35 % routed to species
    // MOZART-T1 does not track (HEMCO-style silent drop).
    { .inventory_species_ = "NMVOC", .mechanism_species_ = "BIGALK",  .scaling_factor_ = 0.30 },
    { .inventory_species_ = "NMVOC", .mechanism_species_ = "BIGENE",  .scaling_factor_ = 0.20 },
    { .inventory_species_ = "NMVOC", .mechanism_species_ = "TOLUENE", .scaling_factor_ = 0.15 },
  };

  SpeciesMap mozart_t1_from_nei{ .mechanism_ = "MOZART-T1" };
  mozart_t1_from_nei.mappings_ = {
    { .inventory_species_ = "NO",  .mechanism_species_ = "NO",  .scaling_factor_ = 1.0 },
    { .inventory_species_ = "NO2", .mechanism_species_ = "NO2", .scaling_factor_ = 1.0 },
    { .inventory_species_ = "CO",  .mechanism_species_ = "CO",  .scaling_factor_ = 1.0 },
    { .inventory_species_ = "SO2", .mechanism_species_ = "SO2", .scaling_factor_ = 1.0 },
    { .inventory_species_ = "NH3", .mechanism_species_ = "NH3", .scaling_factor_ = 1.0 },
    { .inventory_species_ = "PEC", .mechanism_species_ = "BC",  .scaling_factor_ = 1.0 },
    { .inventory_species_ = "POC", .mechanism_species_ = "OC",  .scaling_factor_ = 1.0 },
  };

  SpeciesMap mozart_t1_from_finn{ .mechanism_ = "MOZART-T1" };
  mozart_t1_from_finn.mappings_ = {
    { .inventory_species_ = "CO",   .mechanism_species_ = "CO",   .scaling_factor_ = 1.0 },
    { .inventory_species_ = "NO",   .mechanism_species_ = "NO",   .scaling_factor_ = 1.0 },
    { .inventory_species_ = "NO2",  .mechanism_species_ = "NO2",  .scaling_factor_ = 1.0 },
    { .inventory_species_ = "SO2",  .mechanism_species_ = "SO2",  .scaling_factor_ = 1.0 },
    { .inventory_species_ = "NH3",  .mechanism_species_ = "NH3",  .scaling_factor_ = 1.0 },
    { .inventory_species_ = "BC",   .mechanism_species_ = "BC",   .scaling_factor_ = 1.0 },
    { .inventory_species_ = "OC",   .mechanism_species_ = "OC",   .scaling_factor_ = 1.0 },
  };

  SpeciesMap mozart_t1_from_megan{ .mechanism_ = "MOZART-T1" };
  mozart_t1_from_megan.mappings_ = {
    { .inventory_species_ = "ISOPRENE",     .mechanism_species_ = "ISOP",    .scaling_factor_ = 1.0 },
    { .inventory_species_ = "MYRCENE",      .mechanism_species_ = "BIGALK",  .scaling_factor_ = 1.0 },
    { .inventory_species_ = "TOLUENE",      .mechanism_species_ = "TOLUENE", .scaling_factor_ = 1.0 },
    { .inventory_species_ = "METHANOL",     .mechanism_species_ = "CH3OH",   .scaling_factor_ = 1.0 },
    { .inventory_species_ = "FORMALDEHYDE", .mechanism_species_ = "CH2O",    .scaling_factor_ = 1.0 },
  };

  // --------------------------------------------------------------
  // 2. Dataset descriptor for the legacy CEDS inventory.
  //
  //    ECCAD-conforming files (CAMS, NEI, FINN, MEGAN climatology
  //    below) do not need a descriptor — MIEM's reader knows the
  //    ECCAD convention. Non-ECCAD files need the adapter.
  // --------------------------------------------------------------
  DatasetDescriptor ceds_legacy{
    .variable_prefix_        = "emiss_",
    .flux_units_             = "kg m-2 s-1",
    .unit_conversion_factor_ = 1.0,
    .time_dimension_         = "time",
    .cell_dimension_         = "ncol",
    .species_rename_ = {
      { "emiss_no",  "NO"  },
      { "emiss_co",  "CO"  },
      { "emiss_so2", "SO2" },
    },
  };

  // --------------------------------------------------------------
  // 3. Sources.
  //
  //    All five are `mode: Offline` — v1 scope (see architecture
  //    doc §6). Precedence is encoded in (category, hierarchy):
  //    within one category the highest hierarchy wins per cell;
  //    different categories always sum. Duplicate (category,
  //    hierarchy) across two sources is a load-time error (§7).
  //
  //    Layering here:
  //      category 0 (anthropogenic bucket):
  //        hierarchy 1  CAMS global        — baseline
  //        hierarchy 2  NEI US overlay     — wins over CAMS in US cells
  //        hierarchy 3  CEDS US override   — wins over both
  //      category 1 (fire bucket):
  //        hierarchy 1  FINN fires
  //      category 2 (biogenic bucket):
  //        hierarchy 1  MEGAN offline climo
  // --------------------------------------------------------------
  SourceConfig cams_anthro{
    .name_                   = "cams anthro",
    .mode_                   = SourceMode::Offline,
    .type_                   = SourceType::Anthropogenic,
    .file_pattern_           = "/glade/campaign/acom/emissions/cams-v6.2/"
                               "CAMS-GLOB-ANT_v6.2_{YYYY}-{MM}.nc",
    .convention_             = InventoryConvention::ECCAD,
    .species_map_            = mozart_t1_from_cams,
    .temporal_interpolation_ = TemporalInterpolation::Linear,   // blend between the two file timestamps
                                                                // bracketing the model timestep
    .vertical_injection_     = VerticalInjection::Surface,
    .category_               = 0,
    .hierarchy_              = 1,
    .scaling_factor_         = 1.0,
    .sector_                 = "anthropogenic",
  };

  SourceConfig nei_us_anthro{
    .name_                   = "nei us anthro",
    .mode_                   = SourceMode::Offline,
    .type_                   = SourceType::Anthropogenic,
    .file_pattern_           = "/glade/campaign/acom/emissions/nei-2020/"
                               "NEI_2020_{YYYY}{MM}{DD}.nc",
    .convention_             = InventoryConvention::ECCAD,
    .species_map_            = mozart_t1_from_nei,
    .temporal_interpolation_ = TemporalInterpolation::Linear,
    .vertical_injection_     = VerticalInjection::Surface,
    .category_               = 0,
    .hierarchy_              = 2,
    .scaling_factor_         = 1.0,
    .sector_                 = "anthropogenic",                 // shares a sector label with cams_anthro
                                                                // — diagnostic bucket sums both
  };

  SourceConfig finn_fires{
    .name_                   = "finn fires",
    .mode_                   = SourceMode::Offline,
    .type_                   = SourceType::Fire,
    .file_pattern_           = "/glade/campaign/acom/emissions/finn-2.6/"
                               "FINN_{YYYY}{DDD}.nc",            // DOY token
    .convention_             = InventoryConvention::ECCAD,
    .species_map_            = mozart_t1_from_finn,
    .temporal_interpolation_ = TemporalInterpolation::Nearest,   // daily file; nearest-in-time is fine
    .vertical_injection_     = VerticalInjection::Surface,
    .category_               = 1,
    .hierarchy_              = 1,
    .scaling_factor_         = 1.0,
    .sector_                 = "fire",
  };

  SourceConfig ceds_us_override{
    .name_                   = "ceds us override",
    .mode_                   = SourceMode::Offline,
    .type_                   = SourceType::Anthropogenic,
    .file_pattern_           = "/glade/campaign/acom/emissions/ceds-2021-04-21/"
                               "CEDS_{sector}_{YYYY}.nc",        // sector-templated; one file per sector
    .convention_             = InventoryConvention::Descriptor,
    .descriptor_             = ceds_legacy,
    .species_map_            = mozart_t1_from_cams,              // reuse CAMS map — same inventory species names
    .temporal_interpolation_ = TemporalInterpolation::Linear,
    .vertical_injection_     = VerticalInjection::Surface,
    .category_               = 0,
    .hierarchy_              = 3,                                // wins over cams_anthro AND nei_us_anthro
    .scaling_factor_         = 1.0,
    .sector_                 = "anthropogenic",
  };

  SourceConfig megan_offline_climo{
    .name_                   = "megan offline climo",
    .mode_                   = SourceMode::Offline,
    .type_                   = SourceType::Biogenic,
    .file_pattern_           = "/glade/campaign/acom/emissions/megan-climo/"
                               "MEGAN_climo_{MM}.nc",
    .convention_             = InventoryConvention::ECCAD,
    .species_map_            = mozart_t1_from_megan,
    .temporal_interpolation_ = TemporalInterpolation::Linear,
    .vertical_injection_     = VerticalInjection::Surface,
    .category_               = 2,
    .hierarchy_              = 1,
    .scaling_factor_         = 1.0,
    .sector_                 = "biogenic",
  };

  // --------------------------------------------------------------
  // 4. Aspirational v2/v3 source shapes.
  //
  //    Shown so the API surface is visible. Commented out — each
  //    would raise the named error from EmissionsModule's ctor
  //    under v1. See architecture doc §6 and README open questions.
  //
  //    v2 plume-rise + precomputed 3D fire fluxes:
  // --------------------------------------------------------------
#if 0  // v1: EmissionsModule throws UnsupportedVerticalInjection
  SourceConfig finn_plume_3d{
    .name_                   = "finn plume 3d",
    .mode_                   = SourceMode::Offline,
    .type_                   = SourceType::Fire,
    .file_pattern_           = "/glade/campaign/acom/emissions/finn-2.6-3d/"
                               "FINN_3D_{YYYY}{DDD}.nc",
    .convention_             = InventoryConvention::ECCAD,
    .species_map_            = mozart_t1_from_finn,
    .temporal_interpolation_ = TemporalInterpolation::Nearest,
    .vertical_injection_     = VerticalInjection::Plume,
    .category_               = 1,
    .hierarchy_              = 2,
    .scaling_factor_         = 1.0,
    .sector_                 = "fire",
  };
#endif

  //    v2/v3 online sources — dust, sea-salt, lightning NOx, MEGAN.
  //    All share `mode: Online` + a `provider_` string naming the
  //    external scheme (shape TBD — see README open questions).
#if 0  // v1: EmissionsModule throws OnlineSourcesNotSupported
  SourceConfig dust_online{
    .name_     = "dust online",
    .mode_     = SourceMode::Online,
    .type_     = SourceType::Dust,
    .provider_ = "quacs.dust",
    .species_map_ = SpeciesMap{
      .mechanism_ = "MOZART-T1",
      .mappings_  = {{ .inventory_species_ = "DUST",
                       .mechanism_species_ = "DUST_A1",
                       .scaling_factor_ = 1.0 }},
    },
    .category_ = 30, .hierarchy_ = 1, .scaling_factor_ = 1.0,
    .sector_   = "dust",
  };
  SourceConfig seasalt_online{
    .name_     = "seasalt online",
    .mode_     = SourceMode::Online,
    .type_     = SourceType::SeaSalt,
    .provider_ = "quacs.seasalt",
    .species_map_ = SpeciesMap{
      .mechanism_ = "MOZART-T1",
      .mappings_  = {{ .inventory_species_ = "SEASALT",
                       .mechanism_species_ = "SSLT_A1",
                       .scaling_factor_ = 1.0 }},
    },
    .category_ = 31, .hierarchy_ = 1, .scaling_factor_ = 1.0,
    .sector_   = "seasalt",
  };
  SourceConfig lightning_nox{
    .name_     = "lightning nox",
    .mode_     = SourceMode::Online,
    .type_     = SourceType::Lightning,
    .provider_ = "quacs.lightning",
    .species_map_ = SpeciesMap{
      .mechanism_ = "MOZART-T1",
      .mappings_  = {{ .inventory_species_ = "NO_lightning",
                       .mechanism_species_ = "NO",
                       .scaling_factor_ = 1.0 }},
    },
    .category_ = 32, .hierarchy_ = 1, .scaling_factor_ = 1.0,
    .sector_   = "lightning",
  };
  SourceConfig megan_online{
    .name_        = "megan online",
    .mode_        = SourceMode::Online,
    .type_        = SourceType::Biogenic,
    .provider_    = "quacs.megan",
    .species_map_ = mozart_t1_from_megan,
    .category_    = 2, .hierarchy_ = 2, .scaling_factor_ = 1.0,
    .sector_      = "biogenic",
  };
#endif

  // --------------------------------------------------------------
  // 5. Assemble the MIEMConfig.
  //
  //    Plain struct. No yaml-cpp. No mechanism_configuration types.
  //    No musica types. The order of `sources_` does not imply
  //    precedence — (category, hierarchy) does.
  // --------------------------------------------------------------
  MIEMConfig cfg{
    .version_ = "1.0.0",
    .sources_ = { cams_anthro, nei_us_anthro, finn_fires,
                  ceds_us_override, megan_offline_climo },
  };

  // --------------------------------------------------------------
  // 6. Create the module.
  //
  //    Dimensions come from the host mesh. MPAS-A 120 km global
  //    shown here; any unstructured or structured grid works.
  //
  //    Construction is lazy: no file I/O or NetCDF opens happen
  //    here. Files are opened on the first Run() call that needs
  //    them (see README open question on construct vs first-run
  //    I/O).
  // --------------------------------------------------------------
  const int n_cells       = 163842;
  const int n_vert_levels = 60;
  EmissionsModule module(cfg, n_cells, n_vert_levels);

  // --------------------------------------------------------------
  // 7. Run one timestep.
  //
  //    `sim_time_sec` is absolute seconds since the model epoch —
  //    MIEM uses this to interpolate the input NetCDF time axis,
  //    which is CF-encoded ("seconds since <epoch>"). `dt_sec` is
  //    the host's chemistry timestep.
  // --------------------------------------------------------------
  const double sim_time_sec = 86400.0 * 180.0;  // day 180 of a 2025 run
  const double dt_sec       = 600.0;             // 10 minutes
  EmisState state = module.Run(sim_time_sec, dt_sec);

  // --------------------------------------------------------------
  // 8. Consume outputs.
  //
  //    surface_flux_ :  [n_cells × n_mechanism_species]
  //        Host pushes these to MICM as EMIS.<species> rate params.
  //    tendency_     :  [n_cells × n_vert_levels × n_mechanism_species]
  //        Populated when any source uses VerticalInjection::Plume
  //        (aspirational). Zero-filled for the pure-surface v1
  //        example here.
  //    sector_fluxes_:  map<sector_label, [n_cells × n_species]>
  //        Diagnostic. Two sources sharing "anthropogenic" above
  //        both contribute to that bucket.
  // --------------------------------------------------------------
  std::cout << std::scientific << std::setprecision(3);
  std::cout << "     cell,     species,                 flux [kg m-2 s-1]\n";
  std::cout << "  -------, -----------, -------------------------------\n";
  std::cout << std::setw(9) << 0 << ", " << std::setw(11) << "NO"  << ", "
            << std::setw(31) << state.surface_flux_(0, "NO")  << "\n";
  std::cout << std::setw(9) << 0 << ", " << std::setw(11) << "NO2" << ", "
            << std::setw(31) << state.surface_flux_(0, "NO2") << "\n";
  std::cout << std::setw(9) << 0 << ", " << std::setw(11) << "CO"  << ", "
            << std::setw(31) << state.surface_flux_(0, "CO")  << "\n";
  std::cout << std::setw(9) << 0 << ", " << std::setw(11) << "ISOP" << ", "
            << std::setw(31) << state.surface_flux_(0, "ISOP") << "\n";
  std::cout << "\n";
  std::cout << "sector 'anthropogenic', cell 0, NO (CAMS+NEI+CEDS bucket): "
            << state.sector_fluxes_.at("anthropogenic")(0, "NO") << " kg m-2 s-1\n";
  std::cout << "sector 'fire',          cell 0, CO (FINN):                 "
            << state.sector_fluxes_.at("fire")(0, "CO") << " kg m-2 s-1\n";
  std::cout << "sector 'biogenic',      cell 0, ISOP (MEGAN climo):        "
            << state.sector_fluxes_.at("biogenic")(0, "ISOP") << " kg m-2 s-1\n";

  return 0;
}
