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
install.packages("devtools")
Sys.setenv(PKG_BUILD_EXTRA_FLAGS = "false") # optional, but should increase performance
devtools::install_github("NCI-CISNET/shg-r")
# OR
devtools::install_github("NCI-CISNET/shg-r@[optional-branch-of-your-choice]")
```
## Basic usage
Relying on the default values for input filepaths, RNG strategy, multi-threading, immediate cessation, segments you can launch a smoking history simulation as follows: 
```r
library(SmokingHistoryGenerator)
shg <- new(SHGInterface)
N <- 10^5 # Individuals to simulate (REPEAT)
race = 0 # All races combined
sex = 0 # male
cohort_year = 1940
RNGSTREAM_SIM <- shg$runSimFromFixedValues(N, race, sex, cohort_year)
```

You can also use a pre-generated population instead of using fixed values for race, sex, cohort_year:

If **`birth_cohort` spans many distinct years** (as in this illustration), you need **full** NHIS-style inputs—initiation, cessation, CPD, and mortality tables that include **every cohort column** your population uses. The trimmed CSVs under **`inst/extdata`** in the installed package do **not** cover that; they only bundle a few cohorts for CRAN. Obtain the **complete** table set when it is published on **Zenodo** (link will appear in [Input data: CRAN bundle vs full NHIS set](#input-data-cran-bundle-vs-full-nhis-set)); until then, use a local **`csv-complete`** tree from your NHIS pipeline if you have one.

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

**Performance comparison (1M individuals, 12 cores):**
| Format | Time | Notes |
|--------|------|-------|
| `none` | ~1.1s | No CPD data |
| `sparse` | ~1.3s | Default, recommended |
| `legacy` | ~1.5s | For backwards compatibility |

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

The SHG package provides functions to capture and restore configuration settings, making it easy to reproduce simulations and share configurations. For detailed documentation and examples, see [CONFIG-MANAGEMENT.md](CONFIG-MANAGEMENT.md).

## Additional documentation

### Compiling from source
Please see the [developer readme](dev-readme.md) for instructions on how to compile the package from source.

### Performance optimization
The package is compiled with `-O3` optimization by default. For additional performance gains on your specific machine, you can enable CPU-specific optimizations by adding the following to your `~/.R/Makevars` file:

```makefile
CXX17FLAGS += -march=native
```

This enables CPU-specific instructions (AVX2, AVX-512, etc.) which can improve performance by 5-20% for numerical code. Note that binaries compiled with `-march=native` are not portable to other machines with different CPUs.

### Input data: CRAN bundle vs full NHIS set

The package installs a **small, trimmed** csv-partial subset under `system.file("extdata", package = "SmokingHistoryGenerator")`: `initiation.csv`, `cessation.csv`, `cpd.csv`, `acm.csv`, and `ocm-excl-lung-cancer.csv`. That bundle is sized for **CRAN** and the bundled tests (cohorts **1940**, **1950**, **2010**; race **0**, sex **0**; CPD rows omit all-“.” / non-positive intensity padding).

The **full** NHIS 1965–2016–style parameter tables are **too large for CRAN**. They will be published as a separate download on **Zenodo** (DOI/link to be added here when the record is public). After downloading, you can unpack them under `tests/testdata/NHIS-1965-2016/` for local work. Large trees (`csv-complete/`, `legacy-complete/`) are omitted from the CRAN tarball via `.Rbuildignore`; trimmed **`csv-partial/`** and **`legacy-partial/`** stay in the repo for tests and local benchmarks.

From the source checkout, a benchmark on that partial CSV set (default cohort **2010**, **1,000,000** individuals per timed run, **3** runs) is:

```bash
Rscript tools/benchmark-testdata-2010.R
```

Optional arguments: `N`, `cohort_year`, number of timed `RUNS`, and input folder `csv-partial` (default), `csv-complete`, or **`legacy-complete`** (wide `*.txt` tables under `tests/testdata/NHIS-1965-2016/`; use cohort **1950** to align with `tools/benchmark-1M.R`). Pass a smaller `N` first for a quick smoke test. The script uses the **installed** package (`library()`); run `R CMD INSTALL .` from the repo root after changing C++ code, or set `SHG_BENCHMARK_USE_PKGLOAD=1` to load the source tree via `pkgload::load_all()`.

To **sweep cohorts** (default: every birth year in the initiation header with `year %% 5 == 0`, one million individuals per cohort unless you pass a smaller `N`) and print min/median/max time across cohorts:

```bash
Rscript tools/benchmark-cohort-variance.R
```

Arguments: `N`, `subdir` (`csv-complete` by default), `step` (years, default 5), **`runs_per_cohort`** (repeat timed runs per cohort and average), optional `year_min` and `year_max` (use literal `NA` for an open bound), optional **`immediate_cessation_year`** (7th, default **`0`** = no mandated policy cessation year). Set `SHG_COHORT_BENCH_CSV=/path/to/out.csv` to write a per-cohort table (`immediate_cessation_year`, **`N`**, **`runs_per_cohort`**, `mean_sec`, `sd_sec`, …). To run **two** full sweeps in one go—first with **`0`**, then with a policy year such as **`2050`** (as in some legacy fixture tests)—set `SHG_COHORT_BENCH_COMPARE_IMMEDIATE_CESSATION=2050`.

**HTML bar chart (paired by cohort):** after a two-phase CSV run, set **`SHG_COHORT_BENCH_HTML=/path/to/report.html`** alongside `SHG_COHORT_BENCH_CSV` to write a self-contained HTML page (table + horizontal bars: **policy `immediate_cessation_year`** vs **paired `0`**). Or render an existing CSV:

```bash
Rscript tools/render-cohort-benchmark-html.R results.csv report.html
```

**Vertical chart (SVG):** two **line** series (green = policy `immediate_cessation_year`, blue = `0`) with **dots** (mean across runs) and **whiskers** (min/max of the timed runs). **Horizontal dashed line** for the blue series cohort mean only (no green mean line). Across-cohort **min/max of mean times** for each series come from the CSV (`overall_*` columns) so the chart does not recompute them. Y-axis tick labels use **two decimal places**; axis title **Seconds**; cohort size in the title uses a short form (**1M**, **500K**, …). Set **`SHG_COHORT_BENCH_HTML_VERTICAL=/path/to/chart.html`** with `SHG_COHORT_BENCH_CSV`. Example (1M, 3 runs per cell, legacy tables, paired compare, both HTML outputs):

```bash
export SHG_COHORT_BENCH_CSV=/tmp/cohort.csv
export SHG_COHORT_BENCH_HTML=/tmp/cohort-table.html
export SHG_COHORT_BENCH_HTML_VERTICAL=/tmp/cohort-vertical.html
export SHG_COHORT_BENCH_COMPARE_IMMEDIATE_CESSATION=2050
Rscript tools/benchmark-cohort-variance.R 1000000 legacy-complete 5 3
```

Or render the vertical page from an existing CSV:

```bash
Rscript tools/render-cohort-vertical-chart.R results.csv report-vertical.html
```

**Checked-in example (1M × 3 runs, `csv-complete`, two-phase compare `2050`):** after regenerating with the env vars above, outputs live under `docs/` as `cohort-benchmark-1M.csv`, `cohort-benchmark-1M-table.html`, and `cohort-benchmark-1M-vertical.html`. HTML renderers refuse CSVs whose `N` column is below 1,000,000 unless you set `SHG_COHORT_CHART_ALLOW_SMALL_N=1` (avoids mistaking a 100K smoke sweep for 1M).

To rebuild `inst/extdata/*.csv` from the package’s `tests/testdata/NHIS-1965-2016/csv-complete/` tree:

```bash
Rscript tools/trim-nhis-testdata.R
```

To trim **wide legacy `.txt`** tables in a directory you supply (not the csv-complete workflow), use `tools/trim-default-inputs.R <source_dir>` (see that script’s header).

### Custom data input files
Please see the [data readme](data-readme.md) for filenames, mortality (**ACM** vs **OCM**), `mortality_filename`, and legacy config keys.

### Legacy mode
The Smoking History Generator R wrapper has a legacy mode that allows you to run the generator using a simulation configuration file rather than properties. This is useful if you want to use the same input files as the CLI version of the Smoking History Generator. The legacy mode is accessed through the `LegacyRunWebVersion()` method of the `SHGInterface` class. You can read more about the legacy mode in the [legacy readme](legacy-readme.md).

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
© 2025 CISNET Lung Working Group. All rights reserved.
