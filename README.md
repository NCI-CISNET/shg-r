# Smoking History Generator: R Interface<img src="./man/cisnet-logo.svg" width="100px;" align="right">
  <!-- badges: start -->
  [![R-CMD-check](https://github.com/NCI-CISNET/shg-rcpp/actions/workflows/R-CMD-check-all-OS.yaml/badge.svg)](https://github.com/NCI-CISNET/shg-rcpp/actions/workflows/R-CMD-check-all-OS.yaml)
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
devtools::install_github("NCI-CISNET/shg-rcpp")
# OR
devtools::install_github("NCI-CISNET/shg-rcpp@[optional-branch-of-your-choice]")
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
```r
shg <- new(SHGInterface)
N <- 10^5 # Individuals to simulate (REPEAT)
pop <- list(
    race = rep(0, N),
    sex = sample(x = c(0, 1), size = N, prob = c(0.5, 0.5), replace = TRUE),
    birth_cohort = rep(1930:1949, N / 20)
)

# The following are default configuration values; change as needed
shg$rng_strategy <- "RngStream"
shg$number_of_segments <- 1
shg$run_multi_threaded <- FALSE

RNGSTREAM_SIM_POP <- shg$runSimFromDataFrame(pop)
```

**Note on RNG strategies:**
- **RngStream** (default): Recommended for all use cases, especially multi-segment and parallel simulations. Supports multiple segments and parallel execution while maintaining IID properties.
- **MersenneTwister**: Legacy RNG for backward compatibility. **Restricted to single-segment, non-parallel execution** due to limitations in maintaining IID properties across segments. Attempting to use MersenneTwister with `number_of_segments > 1` or `run_multi_threaded = TRUE` will result in an error.

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
## Additional documentation

### Compiling from source
Please see the [developer readme](dev-readme.md) for instructions on how to compile the package from source.

### Custom data input files
Please see the [data readme](data-readme.md) for instructions on how to use custom input datasets.

### Legacy mode
The Smoking History Generator R wrapper has a legacy mode that allows you to run the generator using input files rather than properties. This is useful if you want to use the same input files as the CLI version of the Smoking History Generator. The legacy mode is accessed through the `LegacyRunWebVersion()` method of the `SHGInterface` class. You can read more about the legacy mode in the [legacy readme](legacy-readme.md).

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

The **software** is released under the [GPL-3](https://choosealicense.com/licenses/gpl-3.0/). The **input datasets** (found in `./inst/inputs
/default/`) are released under the [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/) license.

## Copyright Notice
© 2025 CISNET Lung Working Group. All rights reserved.
