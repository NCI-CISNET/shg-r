# Smoking History Generator: R Interface<img src="./man/figures/cisnet-logo.svg" width="100px;" align="right">
  <!-- badges: start -->
  [![R-CMD-check](https://github.com/NCI-CISNET/shg-r/actions/workflows/R-CMD-check-all-OS.yaml/badge.svg)](https://github.com/NCI-CISNET/shg-r/actions/workflows/R-CMD-check-all-OS.yaml)
  [![License: GPL-3](https://img.shields.io/badge/License-GPL%20v3-blue.svg)](https://choosealicense.com/licenses/gpl-3.0/)
  <!-- badges: end -->

## About
This R package provides a convenient interface to the [CISNET](https://cisnet.cancer.gov/) [Smoking History Generator](https://github.com/NCI-CISNET/shg-cli). It can produce the identical outputs as the command line version (CLI) of the Smoking History Generator in R and offers an easy way for modelers to access the Smoking History Generator directly in R.

## Getting Started

### Installation from CRAN
_Note: Eventually this package will be hosted on CRAN. For now, you must install directly from Github (see below)_.
```r
install.packages("SmokingHistoryGenerator") # Coming soon to CRAN
```

### Installation from Github
```r
install.packages("pak")
pak::pak("NCI-CISNET/shg-r")
# OR
pak::pak("NCI-CISNET/shg-r@[optional-branch-of-your-choice]")
```
## Loading parameter sets

The SHG needs calibrated input files (initiation, cessation, CPD, and mortality tables).
The package ships a small CRAN-sized subset under `inst/extdata/`; full NHIS-style tables
are distributed as **parameter bundles** via Zenodo (and GitHub Releases).

### Traditional workflow: local folder with already-uncompressed files

Use this when you already have a local directory containing `smoking/` and `mortality/`
files and want to point SHG directly at those inputs.

```r
library(SmokingHistoryGenerator)
shg <- new(SHGInterface)

shg$input_data_folder <- "/path/to/usa-national@smok-2016"
shg$initiation_filename <- "smoking/initiation.csv"
shg$cessation_filename  <- "smoking/cessation.csv"
shg$cpd_filename        <- "smoking/cpd.csv"
shg$mortality_filename  <- "mortality/acm.csv"  # or mortality/ocm-excl-lung-cancer.csv

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

Use a single config list that includes both bundle provenance and run fields.
For now this example uses a local zip path; later this can point to a Zenodo URL.

```r
library(SmokingHistoryGenerator)
shg <- new(SHGInterface)

# Local zip path for now (replace with Zenodo URL when published)
zip_path <- "/path/to/usa-national@smok-2016.zip"

run_cfg <- list(
  params_bundle_source = zip_path,
  params_mortality = "acm",   # or "ocm"; alias `mortality = "ocm"` also works
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
  params_bundle_source = "https://zenodo.org/records/xxxx/files/usa-national@smok-2016.zip",
  params_mortality = "acm",
  individuals = 1e5,
  race = 0,
  sex = 0,
  cohort_year = 1980
)
```

The bundle is downloaded/extracted once and cached locally; subsequent calls reuse the cache.
See [`data-readme.md`](data-readme.md) for all supported URL forms, the mortality toggle (ACM vs OCM), private-repo authentication, and cache management.

---

## Basic usage
Relying on the default values for input filepaths, RNG strategy, multi-threading, immediate cessation, segments you can launch a smoking history simulation as follows: 
```r
library(SmokingHistoryGenerator)
shg <- new(SHGInterface)
N <- 10^5 # Individuals to simulate (REPEAT)
race = 0 # All races combined
sex = 0 # male
cohort_year = 1940
run_cfg <- list(
  individuals = N,
  race = race,
  sex = sex,
  cohort_year = cohort_year
)
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

If `birth_cohort` spans many distinct years (as in this illustration), you need **full** NHIS-style inputs—initiation, cessation, CPD, and mortality tables that include every cohort column your population uses. The trimmed CSVs under `inst/extdata` in the installed package do **not** cover that; they only bundle a few cohorts for CRAN. See [data-readme.md](data-readme.md) for full input-data options.

```r
shg <- new(SHGInterface)
# Full tables required for multi-year cohorts—not system.file("extdata", ...):
shg$input_data_folder <- "/path/to/NHIS-1965-2016/csv-complete"
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

If you want to produce identical results as with previous versions of the SHG, you must select the Mersenne Twister strategy:

```r
library(SmokingHistoryGenerator)
shg <- new(SHGInterface)
N <- 10^5 # Individuals to simulate (REPEAT)
# If you want to produce identical results as previous versions of shg-cli you must set the following properties:
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

For CLI-like performance, you can write results directly to disk instead of returning a DataFrame:

```r
shg$output_file <- "/path/to/output.csv"
result <- shg$runSimFromDataFrame(df)
# Result is a small info DataFrame; actual data is in the file
```

File output matches CLI's data format (semicolon-separated) and achieves similar performance (~1.3s for 1M individuals).

## Setting Random Number Generator Seeds

For information on specifying custom seeds for reproducibility, see [RNG-SEEDS.md](RNG-SEEDS.md).

## Configuration Management

The SHG package provides both intent-oriented config APIs (`getConfig()` / `useConfig()`) and reproducibility-focused YAML export (`save_config()`), making it easier to tune runs locally while still sharing exact portable reruns. For detailed documentation and examples, see [CONFIG-MANAGEMENT.md](CONFIG-MANAGEMENT.md).

## Additional documentation

- [Input data and parameter bundles](data-readme.md)  
  (CRAN subset vs full NHIS inputs, `load_params()`, cache behavior, ACM/OCM, custom files)
- [Portable YAML configuration workflow](CONFIG-MANAGEMENT.md)  
  (save/load config for platform-agnostic reproducibility)
- [Developer build and compile guidance](dev-readme.md)  
  (source builds, compile flags, local optimization options)
- [Legacy mode details](legacy-readme.md)

## Contributors
The Smoking History Generator CLI (Command Line Interface) was developed in the early 2000s and maintained by several contributors since that time.
- Original author: Martin Krapcho
- Contributors: Ben Racine, Alexander Gaenko, John Clarke
- R package wrapper author: John Clarke
- Maintainer: John Clarke
- NCI contact: Rocky Feuer

## Publications
You can find a complete set of publications about the Smoking History Generator in the [shg-cli readme](https://github.com/NCI-CISNET/shg-cli/tree/v6.4.0-rc?tab=readme-ov-file#publications).

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
