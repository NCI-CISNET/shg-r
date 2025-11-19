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
If you want to produce identical results as with previous versions of the SHG, you must select the Mersenne Twister strategy and be sure to set the number of segments to 1 and/or run_multi_threaded to FALSE.

```r
library(SmokingHistoryGenerator)
shg <- new(SHGInterface)
N <- 10^5 # Individuals to simulate (REPEAT)
# If you want to produce identical results as previous versions of shg-cli you must set the following properties:
shg$rng_strategy <- "MersenneTwister"
shg$number_of_segments <- 1
shg$run_multi_threaded <- FALSE
MT_SIM <- shg$runSimFromFixedValues(N, 0, 0, 1940)
```

## Setting Random Number Generator Seeds

For reproducibility, you can specify custom seeds for the random number generator. **RngStream is the recommended and default RNG strategy**, especially for multi-threaded simulations.

### RngStream (Recommended)

RngStream uses a single seed vector with 6 elements that generates 4 substreams (one for each stream: initiation, cessation, life table, individual):

```r
library(SmokingHistoryGenerator)
shg <- new(SHGInterface)
shg$input_data_folder <- system.file("inputs/default", "", package="SmokingHistoryGenerator")

# Set a custom seed for RngStream (6-element vector)
# This is a single seed; SHG initiates 4 IID substreams for each segment based on this starting seed
shg$rng_strategy <- "RngStream"  # This is the default
shg$rngstream_seed <- c(12345, 12345, 12345, 12345, 12345, 12345)

N <- 10^5
pop <- list(
    race = rep(0, N),
    sex = sample(x = c(0, 1), size = N, prob = c(0.5, 0.5), replace = TRUE),
    birth_cohort = rep(1930:1949, N / 20)
)
shg$number_of_segments <- 10
shg$run_multi_threaded <- TRUE
smoking_history <- shg$runSimFromDataFrame(pop)
```

### MersenneTwister (Legacy)

MersenneTwister requires 4 separate seeds (one for each stream: initiation, cessation, life table, individual):

```r
library(SmokingHistoryGenerator)
shg <- new(SHGInterface)
shg$input_data_folder <- system.file("inputs/default", "", package="SmokingHistoryGenerator")

# Set custom seeds for MersenneTwister (4-element vector)
# Seeds in order: initiation, cessation, life table, individual
shg$rng_strategy <- "MersenneTwister"
shg$mt_seeds <- c(1898587603, 1468371936, 1551308340, 1590227640)

N <- 10^5
MT_SIM <- shg$runSimFromFixedValues(N, 0, 0, 1940)
```

**Note:** If seeds are not specified, default values will be used automatically.

### Retrieving and Resetting Seeds

You can retrieve the current seed(s) for the selected RNG strategy using `get_current_seeds()`, and reset them to default values using `reset_seeds_to_defaults()`:

```r
library(SmokingHistoryGenerator)
shg <- new(SHGInterface)
shg$input_data_folder <- system.file("inputs/default", "", package="SmokingHistoryGenerator")

# Set custom seeds
shg$rng_strategy <- "RngStream"
shg$rngstream_seed <- c(11111, 22222, 33333, 44444, 55555, 66666)

# Retrieve current seeds
current_seeds <- shg$get_current_seeds()
print(current_seeds)  # Returns the rngstream_seed vector

# Reset to default values
shg$reset_seeds_to_defaults()

# Verify seeds have been reset
default_seeds <- shg$get_current_seeds()
print(default_seeds)  # Returns default RngStream seed: c(12345, 12345, 12345, 12345, 12345, 12345)
```

The `get_current_seeds()` method automatically returns the appropriate seed(s) based on the current `rng_strategy`:
- If `rng_strategy` is `"MersenneTwister"`, it returns `mt_seeds` (4-element vector)
- If `rng_strategy` is `"RngStream"`, it returns `rngstream_seed` (6-element vector)
- Returns an empty vector if seeds have not been explicitly set (defaults will be used)

The `reset_seeds_to_defaults()` method resets the seed(s) to their default values for the currently selected RNG strategy, equivalent to creating a new `SHGInterface` object.

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
