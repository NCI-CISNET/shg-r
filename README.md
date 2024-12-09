# RcppSmokingHistoryGenerator: An R interface to CISNET Smoking History Generator
  <!-- badges: start -->
  [![R-CMD-check](https://github.com/CSNW/rcpp-shg/actions/workflows/R-CMD-check.yaml/badge.svg)](https://github.com/CSNW/rcpp-shg/actions/workflows/R-CMD-check.yaml)
  <!-- badges: end -->

## About
This package is a wrapper for the CISNET Smoking History Generator C++ source code. It can produce the exact outputs as the command line version (CLI) of the Smoking History Generator if needed.

## Installation for end-users from Github
```r
library(Rcpp) # required?
if (!require("Rcpp")) install.packages("Rcpp") # required?
if (!require("devtools")) install.packages("devtools")
Sys.setenv(PKG_BUILD_EXTRA_FLAGS = "false")
devtools::install_github("CSNW/rcpp-shg@[branch-of-your-choice]")
```

## Installation for end-users from CRAN
Under development: Eventually this package will be hosted on CRAN
```r
if (!require("RcppSmokingHistoryGenerator")) install.packages("RcppSmokingHistoryGenerator")
```

## Installation for developers
Retrieve the `rcpp-shg` repository from Github and open an R session.
```r
setwd("path-to-rcpp-shg")
if (!require("devtools")) install.packages("devtools")
Sys.setenv(PKG_BUILD_EXTRA_FLAGS = "false")
devtools::load_all()
```

Then initially and each time you make changes to the src directory
```r
# If you want to prevent the pedantic and -O0 optimization (slower)
Sys.setenv(PKG_BUILD_EXTRA_FLAGS = "false")

# If you want to force a recompile
devtools::clean_dll()

# Note: debug = TRUE results in "(debug build)" and -O0 -g etc. which overrides Makevars
pkgbuild::compile_dll(path = ".", debug = FALSE)

# Recompile the package if necessary (typically after changes to the C++ source)
devtools::load_all() 
library(RcppSmokingHistoryGenerator)
```

# Basic usage

Relying on the default values for input filepaths, RNG strategy, multi-threading, immediate cessation, segments you can launch a smoking history simulation as follows: 
```r
library(RcppSmokingHistoryGenerator)
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
shg$number_of_segments <- 10
shg$run_multi_threaded <- TRUE

RNGSTREAM_SIM_POP <- shg$runSimFromDataFrame(pop)
```
You can also run the simulator using the (legacy) Mersenne Twister RNG.

```r
library(RcppSmokingHistoryGenerator)
shg <- new(SHGInterface)
N <- 10^5 # Individuals to simulate (REPEAT)
shg$rng_strategy <- "MersenneTwister"
# Optionally set the number segments to 1 and disable multi-threaded in order to produce identical results as the CLI
shg$number_of_segments <- 1
shg$run_multi_threaded <- FALSE
RNGSTREAM_SIM <- shg$runSimFromFixedValues(N, 0, 0, 1940)
```

You can also use the `LegacyRunWebVersion()` method which configures the generator using input file (rather than properties) and sends the output to a text file. Two example input files are included with the package. Note that if you use `LegacyRunWebVersion()` none of the properties of `shg` you may have set in R will be taken into consideration. Only the properties that you set in the input file will be considered. Also note that legacy mode runs with a single segment and with no multi-threading.
```r
library(RcppSmokingHistoryGenerator)
shg <- new(SHGInterface)
shg$LegacyRunWebVersion("./inst/inputs/test_input_example_MersenneTwister.txt")
shg$LegacyRunWebVersion("./inst/inputs/test_input_example_RngStream.txt")
```