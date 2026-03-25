# Alternative Data Backends for MIEM

**Status:** Design exploration (2026-03-24)
**Authors:** V. Weeks
**Context:** Evaluate alternatives to NetCDF for MIEM's emission data I/O pipeline,
using combinations of Apache Arrow, Parquet, HDF5 VDS, and DuckDB.

## Motivation

MIEM's I/O is currently coupled to NetCDF-C via ~16 `nc_*` API calls inside
`SESReader`. If NetCDF breaks (library incompatibilities, CMake detection, missing
HPC dependencies), the entire pipeline is dead. Additionally:

- NetCDF's row-major chunking is suboptimal for MIEM's column-oriented access
  pattern (read N species at one timestep across all cells)
- Multi-file management (`{YYYY}{MM}{DD}` token expansion, file switching in
  `OfflineEmissionSource::LoadBrackets`) adds complexity that the storage layer
  could handle
- No zero-copy path from file to `EmisState` — every read requires a buffer copy

The key architectural observation is that **`SESReader` is already an abstraction
boundary** — nothing above it knows about NetCDF. The entire downstream pipeline
(`SpeciesMap`, `TemporalInterpolator`, category/hierarchy aggregation,
`FluxConverter`) operates on `std::vector<Real>` in species-major layout.

## Current I/O Coupling

```
YAML Config
  → OfflineEmissionSource::LoadBrackets()
    → SESReader::Open()        ← NetCDF coupling starts here
    → SESReader::ReadFlux()    ← All nc_* calls live here
    → SESReader::Close()
  → SpeciesMap::Apply()        ← Format-agnostic from here down
  → TemporalInterpolator
  → Category/Hierarchy aggregation
  → FluxConverter → EmisState → C API → Fortran
```

A backend swap only needs to reimplement the `SESReader` contract:

| Method | Contract |
|--------|----------|
| `Open(path, descriptor)` | Open file, detect schema, discover species |
| `QuerySpecies()` | Return available species names |
| `NumTimeSteps()` / `NumCells()` | Return dimension sizes |
| `GetTimeValues()` | Return time coordinate as `vector<double>` |
| `ReadFlux(time_idx, species, flux_out, n_cells_out)` | Fill species-major `(n_species × n_cells)` buffer |

---

## Option 1: Apache Parquet (via Arrow C++)

### What it gives you

- **Column-oriented storage** — each species is a column, ideal for "read species X
  across all cells"
- **Built-in compression** (Snappy, ZSTD, LZ4) — typically 2–5× smaller than
  deflated NetCDF for numeric arrays
- **Row-group partitioning** — one row group per timestep = natural time-slice
  boundaries
- **Predicate pushdown** — skip row groups by time range without reading them
- **Zero-copy reads** via Arrow memory-mapped I/O
- **No external C library dependency** — Arrow C++ is self-contained

### SES-equivalent Parquet layout

```
File: emi_anthro_2024.parquet

Schema (Arrow):
  time:     float64     (not null)   -- CF-encoded time coordinate
  emi_NOx:  float64     (nullable)   -- null replaces _FillValue
  emi_SO2:  float64     (nullable)
  emi_CO:   float64     (nullable)

Row group partitioning:
  One row group per timestep, n_cells rows each.
  Row groups ordered by time.

Key-value metadata (file-level):
  "ses_version":      "2.0"
  "ses_format":       "parquet"
  "flux_units":       "kg m-2 s-1"
  "n_cells":          "48602"
  "grid_description": "CAM-SE ne30np4"
  "source_inventory": "CEDSv2024-04"
  "time_units":       "days since 2000-01-01"
  "calendar":         "standard"
```

### ReadFlux implementation sketch

```cpp
void ParquetReader::ReadFlux(int time_index,
                             const vector<string>& species,
                             vector<Real>& flux_out, int& n_cells_out) {
  auto row_group = parquet_reader_->RowGroup(time_index);
  for (int s = 0; s < species.size(); ++s) {
    auto col = row_group->Column(column_index_[species[s]]);
    auto array = static_pointer_cast<arrow::DoubleArray>(col->ReadAll());
    // Zero-copy into output buffer
    memcpy(&flux_out[s * n_cells_], array->raw_values(),
           n_cells_ * sizeof(double));
  }
  n_cells_out = n_cells_;
}
```

### Tradeoffs

| Pro | Con |
|-----|-----|
| Column-per-species matches MIEM's access pattern perfectly | Arrow C++ is a large dependency |
| Native null bitmask replaces _FillValue convention | Atmospheric science community uses NetCDF natively |
| Automatic compression per-column | UPTEMPO needs a Parquet writer path |
| Well-supported CMake (vcpkg, conda-forge) | No CF conventions ecosystem (udunits, metadata) |

---

## Option 2: HDF5 Virtual Datasets (VDS)

### What it gives you

- **Virtual file assembly** — one VDS file maps regions of multiple physical HDF5
  files into a single logical dataset, without copying data
- **Backward compatibility** — NetCDF-4 files *are* HDF5 under the hood, so
  existing SES files can be referenced directly
- **Eliminates file-switching logic** — one `Open()` call, any time index

### Why this is compelling for MIEM

MIEM resolves file patterns with `{YYYY}{MM}{DD}` tokens in
`OfflineEmissionSource::ResolveFilePath()`, opening/closing different files as
time progresses. With VDS:

```
virtual_emissions.h5  (VDS — metadata only, no data copy)
  emi_NOx(time=0:8759, n_cells=48602) → mapped from:
    emissions_2024_01.nc  time[0:743]
    emissions_2024_02.nc  time[0:671]
    emissions_2024_03.nc  time[0:743]
    ...
```

The `SESReader` opens ONE file; HDF5 transparently fetches from the correct
physical file. This eliminates `ResolveFilePath()` and `LoadBrackets()`
file-switching logic entirely.

### Tradeoffs

| Pro | Con |
|-----|-----|
| Reads existing SES NetCDF-4 files without conversion | Still depends on libhdf5 |
| Eliminates multi-file management in MIEM code | VDS assembly is an extra preprocessing step |
| HDF5 is on every HPC system | HDF5 C API is verbose (needs wrapper) |
| Smallest conceptual change from current design | Classic NetCDF-3 files aren't HDF5 |

---

## Option 3: DuckDB (Embedded Analytical Database)

### What it gives you

DuckDB is an in-process (embedded, no server) columnar analytical database — often
described as "SQLite for analytics." It runs inside the MIEM process, requires no
external service, and has native support for reading Parquet, Arrow IPC, and CSV.

- **SQL or C++ API** for data access — express reads declaratively instead of
  hand-coding hyperslab indexing
- **Columnar execution engine** — automatic vectorized processing, column pruning,
  predicate pushdown
- **Native Parquet/Arrow integration** — reads Parquet files directly, returns
  results as Arrow RecordBatches (zero-copy)
- **Multi-file queries** — can glob over partitioned Parquet datasets
  (`read_parquet('emissions_*.parquet')`) or union multiple sources in a single query
- **In-memory mode** — no disk database needed; DuckDB operates as a query engine
  over external files
- **Lightweight** — single library, ~20 MB, no external dependencies
- **Parallel I/O** — automatically parallelizes reads across row groups and files

### Why DuckDB is interesting for MIEM

MIEM's `ReadFlux(time_index, species_names, ...)` is fundamentally a query:

```sql
SELECT emi_NOx, emi_SO2, emi_CO
FROM read_parquet('emissions_2024.parquet')
WHERE time = 1234.5
```

With DuckDB, instead of hand-managing row-group indexing, hyperslab selection, fill
value masking, and multi-file resolution, you express intent and the query engine
optimizes the physical access. This is especially powerful for:

1. **Multi-file datasets** — DuckDB natively handles partitioned Parquet:
   ```sql
   SELECT emi_NOx, emi_SO2
   FROM read_parquet('emissions_2024_*.parquet', hive_partitioning=true)
   WHERE month = 3 AND time BETWEEN 1000.0 AND 2000.0
   ```
   This replaces MIEM's `ResolveFilePath()` and `LoadBrackets()` entirely.

2. **Species filtering** — column pruning is automatic. Requesting 3 of 50 species
   only reads those 3 columns from Parquet.

3. **Fill value masking** — `COALESCE(emi_NOx, 0.0)` replaces the manual
   `_FillValue` check.

4. **Aggregation queries** — category/hierarchy aggregation could potentially be
   expressed in SQL, though the "highest hierarchy wins per cell per category"
   logic is non-trivial.

5. **Preprocessing pipeline** — DuckDB can transform raw inventory CSVs into
   SES-compliant Parquet in a single pipeline, potentially simplifying or
   complementing UPTEMPO.

### ReadFlux implementation sketch

```cpp
#include <duckdb.hpp>

class DuckDBReader : public EmissionDataReader {
  duckdb::DuckDB db_;           // In-memory, no disk file
  duckdb::Connection conn_;
  std::string parquet_path_;
  int n_cells_;

 public:
  DuckDBReader() : db_(nullptr), conn_(db_) {}  // In-memory database

  void Open(const std::string& path, const DatasetDescriptor& desc) override {
    parquet_path_ = path;
    // Discover schema
    auto result = conn_.Query(
        "SELECT column_name FROM parquet_schema('" + path + "') "
        "WHERE column_name LIKE 'emi_%'");
    // ... populate available_species_ from result

    // Get dimensions
    auto meta = conn_.Query(
        "SELECT COUNT(*) as n_rows FROM read_parquet('" + path + "') "
        "WHERE rowid < (SELECT COUNT(*) FROM read_parquet('" + path + "') "
        "              WHERE time = (SELECT MIN(time) FROM read_parquet('" + path + "')))");
    // ... or read from file metadata
  }

  void ReadFlux(int time_index,
                const std::vector<std::string>& species,
                std::vector<Real>& flux_out, int& n_cells_out) override {
    // Build column list
    std::string cols;
    for (const auto& sp : species) {
      if (!cols.empty()) cols += ", ";
      cols += "COALESCE(emi_" + sp + ", 0.0) AS emi_" + sp;
    }

    // Query — DuckDB handles row group selection, column pruning, decompression
    auto result = conn_.Query(
        "SELECT " + cols + " FROM read_parquet('" + parquet_path_ + "') "
        "WHERE time = (SELECT DISTINCT time FROM read_parquet('" + parquet_path_ + "') "
        "             ORDER BY time LIMIT 1 OFFSET " + std::to_string(time_index) + ")");

    // Extract Arrow-backed result into flux buffer
    auto arrow_result = result->Fetch();  // Returns Arrow RecordBatch
    for (int s = 0; s < species.size(); ++s) {
      auto array = std::static_pointer_cast<arrow::DoubleArray>(
          arrow_result->column(s));
      memcpy(&flux_out[s * n_cells_], array->raw_values(),
             n_cells_ * sizeof(double));
    }
    n_cells_out = n_cells_;
  }
};
```

### DuckDB for the preprocessing pipeline

DuckDB could also serve as the UPTEMPO preprocessing engine:

```sql
-- Convert raw CEDS inventory to SES Parquet
COPY (
  SELECT
    time,
    SUM(CASE WHEN species = 'NOx' THEN flux END) AS emi_NOx,
    SUM(CASE WHEN species = 'SO2' THEN flux END) AS emi_SO2,
    SUM(CASE WHEN species = 'CO'  THEN flux END) AS emi_CO
  FROM read_csv('ceds_raw/*.csv', columns={...})
  GROUP BY time, cell_id
  ORDER BY time, cell_id
) TO 'emissions_ses.parquet'
  (FORMAT PARQUET, ROW_GROUP_SIZE 48602, COMPRESSION 'zstd');
```

This replaces custom Python/C++ preprocessing code with declarative SQL, and
outputs Parquet files that MIEM reads natively.

### DuckDB + HDF5 VDS hybrid

DuckDB does not read HDF5/NetCDF natively, but a community extension exists for
reading HDF5 datasets. Alternatively, DuckDB can register Arrow tables produced by
any reader, enabling a hybrid where:

- Legacy NetCDF/HDF5 files → read via libhdf5 into Arrow → register with DuckDB
- New Parquet files → read natively by DuckDB
- Both queryable through the same SQL interface

### Tradeoffs

| Pro | Con |
|-----|-----|
| Declarative queries replace hand-coded hyperslab logic | New dependency (~20 MB) |
| Automatic parallelism, column pruning, predicate pushdown | SQL overhead for simple single-slice reads |
| Native Parquet + Arrow integration (zero-copy) | No native NetCDF/HDF5 support (needs adapter) |
| Multi-file queries replace ResolveFilePath logic | Query compilation has startup cost |
| Could unify MIEM reader + UPTEMPO preprocessor | Unfamiliar paradigm for atmospheric science |
| In-memory mode, no external server | C++ API less mature than C API |
| Useful for ad-hoc data inspection and debugging | Overkill if only reading single time slices |

### When DuckDB shines vs. when it's overhead

**Strong fit:**
- Multi-file partitioned datasets (monthly/daily Parquet files)
- Complex queries: joins across inventories, aggregations, filtering
- Preprocessing pipeline (raw CSV/inventory → SES Parquet)
- Interactive debugging and data inspection during development
- When MIEM eventually needs to combine emission sources at query time

**Weaker fit:**
- Single-file, single-timestep reads (direct Parquet/Arrow is simpler and faster)
- Environments where adding a 20 MB dependency is unacceptable
- When callers need sub-millisecond latency per read (query parsing overhead)

---

## Comparison Matrix

| Criterion | NetCDF (current) | Parquet/Arrow | HDF5 VDS | DuckDB + Parquet |
|-----------|:---:|:---:|:---:|:---:|
| Read performance (MIEM pattern) | Good | Best | Good | Very Good |
| Dependency weight | Medium | Medium | Light | Light–Medium (~20 MB) |
| HPC availability | Everywhere | Growing | Everywhere | Growing |
| Community adoption (atmos. sci.) | Standard | Emerging | Common | Rare |
| Backward compat with SES 1.0 | N/A | Needs conversion | Reads existing NC4 | Needs conversion |
| Multi-file handling | Manual code | Manual or glob | Native (VDS) | Native (glob/SQL) |
| Zero-copy to EmisState | No | Yes | No | Yes (via Arrow) |
| Testability | Needs fixtures | Arrow in-memory | Needs fixtures | SQL against Arrow |
| Preprocessing story | Separate tool | Arrow writer | VDS assembly | SQL pipeline |
| Query expressiveness | Hyperslab only | Column select | Hyperslab only | Full SQL |
| UPTEMPO integration | Native | Parquet writer | VDS step | Could replace parts |

---

## Recommended Architecture: Phased Approach

### Phase 1 — Extract abstract reader interface

Refactor `SESReader` into an abstract `EmissionDataReader` with the current NetCDF
code moved into `NetCDFReader`. Pure refactor, no new dependencies, no behavior
change. Creates the seam for everything that follows.

```cpp
class EmissionDataReader {
 public:
  virtual ~EmissionDataReader() = default;
  virtual void Open(const std::string& path,
                    const DatasetDescriptor& desc) = 0;
  virtual void Close() = 0;
  virtual std::vector<std::string> QuerySpecies() const = 0;
  virtual int NumTimeSteps() const = 0;
  virtual int NumCells() const = 0;
  virtual std::vector<double> GetTimeValues() const = 0;
  virtual void ReadFlux(int time_index,
                        const std::vector<std::string>& species,
                        std::vector<Real>& flux_out,
                        int& n_cells_out) const = 0;
};

// Factory: selects backend by file extension or config
std::unique_ptr<EmissionDataReader> CreateReader(const std::string& path);
```

### Phase 2 — Add Parquet backend via Arrow C++

Implement `ParquetReader : EmissionDataReader`. New emission inventories from
UPTEMPO can target Parquet directly. Column-per-species storage gives the best
read performance for MIEM's access pattern.

### Phase 3 — Evaluate DuckDB for multi-file and preprocessing

Add `DuckDBReader : EmissionDataReader` for cases where:
- Datasets are partitioned across many Parquet files (monthly, daily)
- Preprocessing needs SQL-expressible transforms
- Ad-hoc data inspection is valuable during development

DuckDB wraps Parquet access, so Phase 2's Parquet files work with both readers.
The choice between direct Arrow and DuckDB becomes a configuration option.

### Phase 4 — HDF5 VDS for legacy compatibility

For operational environments with existing NetCDF-4 inventories, add VDS support
so monthly files can be virtually concatenated without conversion. This is an
incremental addition that coexists with Parquet/DuckDB paths.

### Phase 5 (optional) — Arrow as in-memory interchange

If profiling shows the `ReadFlux → vector<Real> → SpeciesMap` copy chain is a
bottleneck, switch the internal buffer to `arrow::RecordBatch` for zero-copy from
Parquet/DuckDB results.

---

## Open Questions

1. **UPTEMPO output format** — Should UPTEMPO produce Parquet natively, or
   continue producing NetCDF with a conversion step? DuckDB could serve as the
   conversion engine.

2. **SES 2.0 scope** — Should a Parquet-based SES convention be formally
   specified, or should Parquet be treated as an implementation detail behind
   the `EmissionDataReader` interface?

3. **HPC deployment** — Arrow/DuckDB availability on Cheyenne/Derecho via
   conda-forge or spack. Need to verify build compatibility.

4. **Runtime selection** — Should the backend be chosen by file extension
   (`.nc` → NetCDF, `.parquet` → Arrow/DuckDB), by config YAML, or both?

5. **DuckDB query caching** — For repeated reads of the same file (bracket
   interpolation reads two adjacent timesteps), does DuckDB's buffer manager
   cache row groups across queries? Initial testing suggests yes, but needs
   benchmarking against direct Arrow reads.
