#####################
Inventory Conventions
#####################

Every emission ``miem::Source`` names an *inventory convention* (its
``convention_`` field) that tells MIEM how the source's NetCDF files are laid
out on disk. The convention is a contract about variable names, dimensions,
and units -- not about the science of the inventory itself. In v1 MIEM
supports a single convention, ``"eccad"``; any other value is rejected as
``UnknownConvention`` when the emissions builder builds the runtime.

Why ECCAD
=========

MIEM's reference emission inventories are distributed through ECCAD
:cite:`ECCAD` (Emissions of atmospheric Compounds and Compilation of Ancillary
Data, https://eccad.aeris-data.fr/), the atmospheric-emissions data repository
hosted by `AERIS <https://www.aeris-data.fr/>`_. Datasets such as
CAMS-GLOB-ANT (anthropogenic) and GFAS (biomass burning) are published there in
a consistent NetCDF layout, and that layout is what MIEM's ``"eccad"``
convention encodes. Standardizing on the ECCAD layout means a host model can
point MIEM at a freshly downloaded inventory without writing a bespoke reader
for each product.

This is a *file-format* dependency, not a network or runtime dependency: MIEM
never contacts ECCAD. A user (or musica's translator) downloads the files
ahead of time and hands MIEM a concrete, already-resolved path through
``Source::file_pattern_``.

What the convention assumes
===========================

The ``"eccad"`` convention expects each file to provide, per emitted species:

- a flux variable keyed by the inventory's species name,
- a mass-flux unit of ``kg m-2 s-1`` (the canonical ECCAD emission unit),
- a ``time`` dimension MIEM interpolates across successive slices, and
- a horizontal cell dimension already on the target grid (v1 does no
  regridding).

The inventory's species names rarely match the chemical mechanism's names.
That gap is closed *after* the file is read, by the source's ``SpeciesMap``,
which rewrites inventory-named emission flux onto the mechanism species MICM
solves for.

Non-conforming files
====================

Inventories that do not follow the ECCAD layout (legacy CEDS files, custom
research products) are not read with a second hard-coded convention. Instead,
upstream tooling attaches a *dataset descriptor* that names the variable
prefix, units, and dimension names so the file can be adapted to the canonical
layout. The descriptor mechanism is defined in the configuration schema and
resolved by musica before MIEM runs; the load-time versus runtime split is
described in the configuration architecture note (``docs/config-architecture.md``
in the repository). For v1, only ``"eccad"`` is accepted directly.
