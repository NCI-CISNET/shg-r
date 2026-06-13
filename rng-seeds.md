# Setting Random Number Generator Seeds

For reproducibility, you can specify custom seeds for the random number generator. **RngStream is the recommended and default RNG strategy**, especially for multi-threaded simulations.

## RngStream (Recommended)

RngStream uses a single seed vector with 6 elements that generates 4 substreams (one for each stream: initiation, cessation, life table, individual):

```r
library(SmokingHistoryGenerator)
shg <- new(SHGInterface)
# This example uses many birth cohorts (1930–1949). You must point input_data_folder
# at full NHIS-style tables with all cohort columns—not the small inst/extdata bundle.
# Full public bundle: coming soon on Zenodo (see README “Input data: CRAN bundle vs full NHIS set”).
shg$input_data_folder <- "/path/to/2018/csv-complete"

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
shg$num_threads <- -1  # -1 = auto (all cores), 1 = single-threaded
smoking_history <- shg$runSimFromDataFrame(pop)
```

## MersenneTwister (Legacy)

MersenneTwister requires 4 separate seeds (one for each stream: initiation, cessation, life table, individual):

```r
library(SmokingHistoryGenerator)
shg <- new(SHGInterface)
shg$input_data_folder <- system.file("extdata", "2018", package="SmokingHistoryGenerator")

# Set custom seeds for MersenneTwister (4-element vector)
# Seeds in order: initiation, cessation, life table, individual
shg$rng_strategy <- "MersenneTwister"
shg$mt_seeds <- c(1898587603, 1468371936, 1551308340, 1590227640)

N <- 10^5
MT_SIM <- shg$runSimFromFixedValues(N, 0, 0, 1940)
```

**Note:** If seeds are not specified, default values will be used automatically.

## Retrieving and Resetting Seeds

You can retrieve the current seed(s) for the selected RNG strategy using `get_current_seeds()`, and reset them to default values using `reset_seeds_to_defaults()`:

```r
library(SmokingHistoryGenerator)
shg <- new(SHGInterface)
shg$input_data_folder <- system.file("extdata", "2018", package="SmokingHistoryGenerator")

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

