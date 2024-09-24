# RcppSmokingHistoryGenerator

## Installation for developers
Retrieve the  rcpp-shg` repo and open an R session in R (or R Studio)
Dependencies:
```r
install.packages("devtools")
install.packages("Rcpp")
install.packages("rstream")
install.packages("rbenchmark")
```

Ensure libraries are loaded
```r
library(Rcpp)
library(rstream)
```

Then initially and each time you make changes to the src directory
```r

devtools::load_all() # recompiles the package if necessary
library(RcppSmokingHistoryGenerator)  # Package name
```

# Example usage
```r
library(RcppSmokingHistoryGenerator)
shg <- new(SHGInterface) # Class
histories <- shg$runSim(10)
histories <- shg$runSim(10) # should be different (continues with same streams)
shg$initialize() # resets seeds for all streams
histories <- shg$runSim(10) # should be same as first instance of test
```

Or use the rstream package to generate the numbers:
```r
library(RcppSmokingHistoryGenerator)
shg <- new(SHGInterface) # Class

# Eventually we hope to instantiate these inside the RCPP interface rather than require them to be passed through from R.
rng1 <- new("rstream.mrg32k3a")
rng2 <- rstream.clone(rng1)
rstream.nextsubstream(rng2)
rng3 <- rstream.clone(rng2)
rstream.nextsubstream(rng3)
rng4 <- rstream.clone(rng3)
rstream.nextsubstream(rng4)

shg$initialize() # instantiates the Smoking_Simulator object in the interface

shg$setRNGtype("MT") # Use SHG internal Mersenne Twister (indepenent from R's RNG)
MT_histories <- shg$runSim(1000) # should be different (continues with same streams)

shg$setRNGs(rng1, rng2, rng3, rng4)
shg$setRNGtype("rstream") # use MRG32k3a with substreams and batch generation
MRG_histories <- shg$runSim(1000)
```

We can benchmark the two approaches like this
```R
# Benchmark the 2 approaches (after instantiating the objects above)
library(rbenchmark)
n <- 100000
results <- benchmark(
  SIM_MT = {
    shg$setRNGtype("MT")
    MT_SIM <- shg$runSim(n)
  },
  SIM_RSTREAM = {
    shg$setRNGtype("rstream")
    RSTREAM_SIM <- shg$runSim(n)
  },
  order = "relative",
  replications = 2 # Number of times to run each expression
)

# Benchmark results
results <- results[, c("test", "elapsed", "relative")]
print(results)
```


# Example file
Consider running the `TestRunWithRstream.R` to see how we can instantiate the interface (and subsequently SHG) and run benchmarks to compare RNG approaches.
