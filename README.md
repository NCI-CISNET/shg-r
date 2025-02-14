# SmokingHistoryGenerator<img src="./man/cisnet-logo.svg" width="100px;" align="right">
  <!-- badges: start -->
  [![R-CMD-check](https://github.com/NCI-CISNET/shg-rcpp/actions/workflows/R-CMD-check.yaml/badge.svg)](https://github.com/CSNW/rcpp-shg/actions/workflows/R-CMD-check.yaml)
  [![License: GPL-3](https://img.shields.io/badge/License-GPL%20v3-blue.svg)](https://choosealicense.com/licenses/gpl-3.0/)
  <!-- badges: end -->
### Rcpp Smoking History Generator: An R interface to the CISNET Smoking History Generator

## About
This package is a wrapper for the [CISNET](https://cisnet.cancer.gov/) [Smoking History Generator](https://github.com/CSNW/smoking-history-generator) C++ source code. It can produce the identical outputs as the command line version (CLI) of the Smoking History Generator in R. It offers an easy way for modelers to access the Smoking History Generator directly in R.

## Getting Started

### Installation for end-users from Github
```r
install.packages("devtools")
Sys.setenv(PKG_BUILD_EXTRA_FLAGS = "false") # optional, but should increase performance
devtools::install_github("NCI-CISNET/shg-rcpp@[optional-branch-of-your-choice]")
```

### Installation for end-users from CRAN (coming soon)
Under development: Eventually this package will be hosted on CRAN
```r
install.packages("SmokingHistoryGenerator")  # Not working yet. Coming soon!
```

## Installation for developers
Retrieve the `rcpp-shg` repository from Github and open an R session.
```r
setwd("path-to-rcpp-shg")
install.packages("devtools")
Sys.setenv(PKG_BUILD_EXTRA_FLAGS = "false") # optional, but should increase performance
devtools::load_all()
```

Then initially and each time you make changes to the src directory
```r
# If you want to prevent the pedantic and -O0 optimization flags (slower)
Sys.setenv(PKG_BUILD_EXTRA_FLAGS = "false")

# If you want to force a recompile
devtools::clean_dll()

# Note: debug = TRUE results in "(debug build)" and -O0 -g etc. which overrides Makevars
pkgbuild::compile_dll(path = ".", debug = FALSE)

# Recompile the package if necessary (typically after changes to the C++ source)
devtools::load_all() 
library(SmokingHistoryGenerator)
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
shg$number_of_segments <- 10
shg$run_multi_threaded <- TRUE

RNGSTREAM_SIM_POP <- shg$runSimFromDataFrame(pop)
```
You can also run the simulator using the (legacy) Mersenne Twister RNG.

```r
library(SmokingHistoryGenerator)
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
library(SmokingHistoryGenerator)
shg <- new(SHGInterface)
shg$LegacyRunWebVersion("./inst/inputs/examples/test_input_example_MersenneTwister.txt")
shg$LegacyRunWebVersion("./inst/inputs/examples/test_input_example_RngStream.txt")
```

## Contributors
The Smoking History Generator CLI (Command Line Interface) was developed in the early 2000s and maintained by several contributors since that time.
- Original author: Martin Krapcho
- Contributors: Ben Racine, Alexander Gaenko, John Clarke
- R package wrapper author: John Clarke
- Maintainer: John Clarke
- NCI contact: Rocky Feuer

## Publications
Multiple manuscripts based on the Smoking History Generator have been published over the years.

- [Tobacco Control and the Reduction in Smoking-Related Premature Deaths in the United States, 1964-2012](https://resources.cisnet.cancer.gov/projects/#shg/tcpd)
- [Patterns of Birth Cohort-Specific Smoking Histories, 1965-2009](https://resources.cisnet.cancer.gov/projects/#shg/tce)
- [Public Health Implications of Raising the Minimum Age of Legal Access to Tobacco Products](https://resources.cisnet.cancer.gov/projects/#shg/iomr)
- [Smoking and Lung Cancer Mortality in the United States From 2015 to 2065: A Comparative Modeling Approach](https://resources.cisnet.cancer.gov/projects/#shg/sbc2)

## Related publications pertaining to RngStream

- [Good Parameter Sets for Combined Multiple Recursive Random Number Generators (1999)](https://pubsonline.informs.org/doi/10.1287/opre.47.1.159)
- [An Objected-Oriented Random-Number Package with Many Long Streams and Substreams (2002)](https://pubsonline.informs.org/doi/10.1287/opre.50.6.1073.358)

## Funding
Funding for the CISNET Smoking History Generator and its Rcpp wrapper came from the following National Cancer Institute (NCI) grants.
- Grant 1
- Grant 2
- Grant 3

## License
This package is released under the [GPL-3]((https://choosealicense.com/licenses/gpl-3.0/)), as is the [Smoking History Generator](https://github.com/CSNW/smoking-history-generator) C++ source code.