# Smoking History Generator: R Interface<img src="./man/figures/cisnet-logo.svg" width="100px;" align="right">

[![R-CMD-check](https://github.com/NCI-CISNET/shg-r/actions/workflows/R-CMD-check-all-OS.yaml/badge.svg)](https://github.com/NCI-CISNET/shg-r/actions/workflows/R-CMD-check-all-OS.yaml)
[![License: GPL-3](https://img.shields.io/badge/License-GPL%20v3-blue.svg)](https://choosealicense.com/licenses/gpl-3.0/)

## About

[SmokingHistoryGenerator](https://CRAN.R-project.org/package=SmokingHistoryGenerator) is the CISNET Smoking History Generator for R: a microsimulation engine that produces individual smoking histories (initiation, cessation, cigarettes per day, mortality) from calibrated NHIS-style input tables.

## Installation

```r
install.packages("SmokingHistoryGenerator")
```

## Quick start

The package ships a small default parameter set (`inst/extdata/2018/`) with cohort columns **1940, 1950, and 2010**. No extra configuration is required for a basic run:

```r
library(SmokingHistoryGenerator)

# Defaults: bundled inputs, 1000 individuals, race=0, sex=0
shg_run(new(SHGInterface), list(cohort_year = 1950))
```

## Loading parameter sets

The SHG needs calibrated input files (initiation, cessation, CPD, and mortality tables).
The package ships a **default** CRAN-sized NHIS-1965–2018 csv-partial under `inst/extdata/2018/` (`smok/`, `mort/`). Full NHIS-style tables
are distributed as **parameter bundles** via Zenodo (and GitHub Releases). See `?shg_load_params` for bundle URLs, ACM vs OCM mortality, authentication, and cache behavior.

### Traditional workflow: local folder with already-uncompressed files

Use this when you already have a local directory containing `smok/` and `mort/`
files and want to point SHG directly at those inputs.

```r
library(SmokingHistoryGenerator)
shg <- new(SHGInterface)

shg$input_data_folder   <- "/path/to/usa-national@smok-2018-mort-2016"
shg$initiation_filename <- "smok/initiation.csv"
shg$cessation_filename  <- "smok/cessation.csv"
shg$cpd_filename        <- "smok/cpd.csv"
shg$mortality_filename  <- "mort/acm.csv"  # or mort/ocm-excl-lung-cancer.csv

run_cfg <- list(
  individuals = 1e5,
  race = 0,
  sex = 0,
  cohort_year = 1980
)
bundle <- shg$runSim(run_cfg)
sim <- bundle$results
```

### Recommended workflow: config list + bundle zip path

Use a single config list that includes both smoking and mortality bundle sources and run fields.

```r
library(SmokingHistoryGenerator)
shg <- new(SHGInterface)

run_cfg <- list(
  smok_params_source = "/path/to/usa-national@smok-NHIS-2022.zip",
  mort_params_source = "/path/to/usa-national@mort-v1.0.0.zip",
  mort_params_type = "acm",   # or "ocm"; alias `mortality = "ocm"` also works
  individuals = 1e5,
  race = 0,
  sex = 0,
  cohort_year = 1980
)

# Hydrate tables from bundle metadata in config
shg_apply_config(shg, run_cfg)

# Single run call returns coupled outputs
bundle <- shg$runSim(run_cfg)
sim <- bundle$results
```

Future Zenodo variant (same pattern; replace `xxxx` with the published record id):

```r
run_cfg <- list(
  smok_params_source = "https://zenodo.org/records/xxxx/files/usa-national@smok-NHIS-2022.zip",
  mort_params_source = "https://zenodo.org/records/xxxx/files/usa-national@mort-v1.0.0.zip",
  mort_params_type = "acm",
  individuals = 1e5,
  race = 0,
  sex = 0,
  cohort_year = 1980
)
```

The bundle is downloaded/extracted once and cached locally; subsequent calls reuse the cache.

---

## Basic usage

Using a config list that includes a parameter bundle source (recommended), you can launch a smoking history simulation as follows:

```r
library(SmokingHistoryGenerator)
shg <- new(SHGInterface)

N <- 10^5
race <- 0
sex <- 0
cohort_year <- 1940
run_cfg <- list(
  smok_params_source = "/path/to/usa-national@smok-NHIS-2022.zip",
  mort_params_source = "/path/to/usa-national@mort-v1.0.0.zip",
  mort_params_type = "acm",
  individuals = N,
  race = race,
  sex = sex,
  cohort_year = cohort_year
)

# Hydrate parameter tables from config bundle metadata
shg_apply_config(shg, run_cfg)

bundle <- shg$runSim(run_cfg)
RNGSTREAM_SIM <- bundle$results
```

For a single object that couples simulated rows with **`original_config`**, **`repro_config`** (full snapshot), and **`run_info`** (machine/software audit), call the 6-argument method with **`attach_run_info = TRUE`**:

```r
bundle <- shg$runSim(run_cfg)

sim <- bundle$results
cfg_intent <- bundle$original_config
cfg_repro <- bundle$repro_config
audit <- bundle$run_info
```

## Common use cases

### 1) Apply a sparse config safely (defaults-first)

```r
shg <- new(SHGInterface)
shg_apply_config(shg, list(cohort_year = 1950))
```

`shg_apply_config()` resets the instance to factory defaults first, then applies only the keys you supply.

### 2) Save sparse intent or full repro config with one writer

```r
# Small hand-editable config snippet
shg_write_config_yaml(bundle$original_config, "intent.yml")

# Full replay config
shg_write_config_yaml(bundle$repro_config, "repro.yml")
```

The same `shg_write_config_yaml(config, path)` function handles both.

### 3) Re-run from a full repro config in-memory (no file)

```r
shg2 <- new(SHGInterface)
shg_apply_config(shg2, bundle$repro_config)
sim2 <- shg2$runSim(bundle$repro_config)
sim2_df <- sim2$results
```

### 4) Load once, then change cohort for another run

```r
shg3 <- new(SHGInterface)
base_run <- shg_load_config(shg3, "repro.yml")  # applies params + engine settings

# Keep everything else the same, change only cohort year
base_run$cohort_year <- 2000

sim3 <- shg3$runSim(base_run)
sim3_df <- sim3$results
```

You can also use a pre-generated population instead of using fixed values for race, sex, cohort_year:

If `birth_cohort` spans many distinct years (as in this illustration), you need **full** NHIS-style inputs—initiation, cessation, CPD, and mortality tables that include every cohort column your population uses. The trimmed CSVs under `inst/extdata/2018` do **not** cover that; they only bundle a few cohorts for CRAN. Use `shg_load_params()` or set `input_data_folder` to a directory with complete tables.

```r
shg <- new(SHGInterface)
# Full tables required for multi-year cohorts—not system.file("extdata", "2018", ...):
shg$input_data_folder <- "/path/to/2018/csv-complete"
N <- 10^5 # Individuals to simulate (REPEAT)
pop <- list(
    race = rep(0, N),
    sex = sample(x = c(0, 1), size = N, prob = c(0.5, 0.5), replace = TRUE),
    birth_cohort = rep(1930:1949, N / 20)
)

# The following are default configuration values; change as needed
shg$rng_strategy <- "RngStream"
shg$number_of_segments <- -1  # -1 = auto, or set explicit value for reproducibility
shg$num_threads <- -1         # -1 = auto (all cores), 1 = single-threaded

RNGSTREAM_SIM_POP <- shg$runSimFromDataFrame(pop)
```

**Note on RNG strategies:**

- **RngStream** (default): Recommended for all use cases, especially multi-segment and parallel simulations. Supports multiple segments and multi-threading while maintaining IID properties.
- **MersenneTwister**: Legacy RNG for backward compatibility. **Restricted to single-segment, single-threaded execution** due to limitations in maintaining IID properties across segments. Attempting to use MersenneTwister with `number_of_segments > 1` or `num_threads != 1` will result in an error.

If you want to produce identical results as with legacy versions of the SHG command line version (v6.3.5 and earlier), you must select the Mersenne Twister strategy:

```r
library(SmokingHistoryGenerator)
shg <- new(SHGInterface)
N <- 10^5 # Individuals to simulate (REPEAT)
# If you want to produce identical results as previous versions of the legacy CLI you must set the following properties:
shg$rng_strategy <- "MersenneTwister"
# Note: MersenneTwister is automatically restricted to 1 segment and non-parallel execution
MT_SIM <- shg$runSimFromFixedValues(N, 0, 0, 1940)
```

## CPD Output Format

The `cpd_format` property controls how cigarettes-per-day data is returned:

```r
shg$cpd_format <- "sparse"  # Default - fastest with CPD: "20, 20, 10, 3"
shg$cpd_format <- "none"    # Fastest - no CPD column returned
shg$cpd_format <- "legacy"  # Backwards compatible: "17 (20), 18 (20), 19 (10)"
```

**Note:** The `sparse` format stores only CPD values. The age can be computed as `init_age + index` since values are sequential from initiation age.

## File Output Mode

For CLI-like performance, you can write rows directly to disk. With the bundled return form, the in-memory object still includes configs and audit metadata, but not the full simulated row set (to conserve memory):

```r
library(SmokingHistoryGenerator)
shg <- new(SHGInterface)

run_cfg <- list(
  smok_params_source = "/path/to/usa-national@smok-NHIS-2022.zip",
  mort_params_source = "/path/to/usa-national@mort-v1.0.0.zip",
  mort_params_type = "acm",
  cohort_year = 1950,
  output_file = "/path/to/output-fixed.csv"
)

# Load parameters from config metadata, then run
bundle <- shg$runSim(run_cfg)

# Same bundle structure; output rows are in output-fixed.csv
# Defaults used here: individuals = 1000, race = 0, sex = 0.
# bundle$original_config / bundle$repro_config / bundle$run_info are returned
```

File output matches CLI's data format (semicolon-separated).

## Random number generator seeds

Set seeds on the `SHGInterface` before running (for example `shg$seed_init`, `shg$seed_cess`, `shg$seed_mortality`, `shg$seed_misc`). Use `getReproConfig()` after a run to inspect the effective values used. See `?SHGInterface` and `?getReproConfig`.

## Configuration management

Use `shg_apply_config()` for intent-oriented updates, `getConfig()` / `useConfig()` to read or replace settings, and `shg_write_config_yaml()` / `shg_load_config()` to save or reload portable YAML for exact reruns.

## Contributors

The Smoking History Generator CLI (Command Line Interface) was developed in the early 2000s and maintained by several contributors since that time.

- Original author: Martin Krapcho
- Contributors: Ben Racine, Alexander Gaenko, John Clarke
- R package wrapper author: John Clarke
- Maintainer: John Clarke
- NCI contact: Rocky Feuer

## Publications

You can find a complete set of publications about the Smoking History Generator via [CISNET](https://cisnet.cancer.gov/) and project-specific resource pages linked from there.

## Funding

Funding for the CISNET Smoking History Generator and the Rcpp wrapper came from the following National Cancer Institute (NCI) grants.

- U01CA253858
- U01CA199284
- U01CA152956
- U01CA097415

## License

You may not use the Software or Datasets for commercial purposes without prior written consent from the CISNET Lung Working Group and without entering into a separate license agreement regarding such commercial use. Contact: Rafael Meza Rodriguez [rmeza@bccrc.ca](mailto:rmeza@bccrc.ca) and Jamie Tam [jamie.tam@yale.edu](mailto:jamie.tam@yale.edu).

The **software** is released under the [GPL-3](https://choosealicense.com/licenses/gpl-3.0/). The **test input tables** shipped with the package are released under the [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/) license.

## Copyright Notice

© 2026 CISNET Lung Working Group. All rights reserved.
