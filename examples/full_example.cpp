// SPDX-License-Identifier: Apache-2.0
//
// Worked MIEM API example.
//
// Constructs a miem::MIEMConfig programmatically and runs one emissions
// timestep. Mirrors how a non-musica caller (ad-hoc host integration,
// test harness, standalone tool) feeds MIEM. Musica's YAML-to-MIEMConfig
// path is shown in musica's examples, not here — by design MIEM never
// sees YAML. Species mapping and inventory translation live upstream in
// MechanismConfiguration and musica::Translate(); by the time MIEMConfig
// reaches MIEM, species are already resolved.
//
// See docs/config-architecture.md §3 for struct ownership and §5 for
// the public API shape. See examples/README.md for a prose walkthrough
// and the open-design-questions list this example surfaces.
//
// NOTE: The miem/ headers included below are stubs that define the API
// surface and compile cleanly but contain no real implementation.
// Output values are placeholders. See the README for details.

#include <miem/config.hpp>            // MIEMConfig, SourceConfig + enums
#include <miem/emissions_module.hpp>  // EmissionsModule
#include <miem/emis_state.hpp>        // EmisState

#include <iomanip>
#include <iostream>
#include <vector>

using namespace miem;

int main()
{
  // --------------------------------------------------------------
  // 1. Sources.
  //
  //    All five are `mode: Offline` — v1 scope (see architecture
  //    doc §6). Precedence is encoded in (category, hierarchy):
  //    within one category the highest hierarchy wins per cell;
  //    different categories always sum. Duplicate (category,
  //    hierarchy) across two sources is a load-time error (§7).
  //
  //    Species mapping and inventory translation are handled upstream
  //    by MechanismConfiguration and musica::Translate() before this
  //    config reaches MIEM.
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
    .temporal_interpolation_ = TemporalInterpolation::Linear,
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
    .temporal_interpolation_ = TemporalInterpolation::Linear,
    .vertical_injection_     = VerticalInjection::Surface,
    .category_               = 0,
    .hierarchy_              = 2,
    .scaling_factor_         = 1.0,
    .sector_                 = "anthropogenic",
  };

  SourceConfig finn_fires{
    .name_                   = "finn fires",
    .mode_                   = SourceMode::Offline,
    .type_                   = SourceType::Fire,
    .file_pattern_           = "/glade/campaign/acom/emissions/finn-2.6/"
                               "FINN_{YYYY}{DDD}.nc",
    .temporal_interpolation_ = TemporalInterpolation::Nearest,
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
                               "CEDS_{sector}_{YYYY}.nc",
    .temporal_interpolation_ = TemporalInterpolation::Linear,
    .vertical_injection_     = VerticalInjection::Surface,
    .category_               = 0,
    .hierarchy_              = 3,
    .scaling_factor_         = 1.0,
    .sector_                 = "anthropogenic",
  };

  SourceConfig megan_offline_climo{
    .name_                   = "megan offline climo",
    .mode_                   = SourceMode::Offline,
    .type_                   = SourceType::Biogenic,
    .file_pattern_           = "/glade/campaign/acom/emissions/megan-climo/"
                               "MEGAN_climo_{MM}.nc",
    .temporal_interpolation_ = TemporalInterpolation::Linear,
    .vertical_injection_     = VerticalInjection::Surface,
    .category_               = 2,
    .hierarchy_              = 1,
    .scaling_factor_         = 1.0,
    .sector_                 = "biogenic",
  };

  // --------------------------------------------------------------
  // 2. Aspirational v2/v3 source shapes.
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
    .temporal_interpolation_ = TemporalInterpolation::Nearest,
    .vertical_injection_     = VerticalInjection::Plume,
    .category_               = 1,
    .hierarchy_              = 2,
    .scaling_factor_         = 1.0,
    .sector_                 = "fire",
  };
#endif

  //    v2/v3 online sources — dust, sea-salt, lightning NOx, MEGAN.
#if 0  // v1: EmissionsModule throws OnlineSourcesNotSupported
  SourceConfig dust_online{
    .name_     = "dust online",
    .mode_     = SourceMode::Online,
    .type_     = SourceType::Dust,
    .provider_ = "quacs.dust",
    .category_ = 30, .hierarchy_ = 1, .scaling_factor_ = 1.0,
    .sector_   = "dust",
  };
  SourceConfig seasalt_online{
    .name_     = "seasalt online",
    .mode_     = SourceMode::Online,
    .type_     = SourceType::SeaSalt,
    .provider_ = "quacs.seasalt",
    .category_ = 31, .hierarchy_ = 1, .scaling_factor_ = 1.0,
    .sector_   = "seasalt",
  };
  SourceConfig lightning_nox{
    .name_     = "lightning nox",
    .mode_     = SourceMode::Online,
    .type_     = SourceType::Lightning,
    .provider_ = "quacs.lightning",
    .category_ = 32, .hierarchy_ = 1, .scaling_factor_ = 1.0,
    .sector_   = "lightning",
  };
  SourceConfig megan_online{
    .name_        = "megan online",
    .mode_        = SourceMode::Online,
    .type_        = SourceType::Biogenic,
    .provider_    = "quacs.megan",
    .category_    = 2, .hierarchy_ = 2, .scaling_factor_ = 1.0,
    .sector_      = "biogenic",
  };
#endif

  // --------------------------------------------------------------
  // 3. Assemble the MIEMConfig.
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
  // 4. Create the module.
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
  // 5. Run one timestep.
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
  // 6. Consume outputs.
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
