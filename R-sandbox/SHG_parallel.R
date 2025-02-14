library(Rcpp)
library(rbenchmark)
library(rstream)
library(parallel)
library(dplyr)  # For bind_rows to combine DataFrames

devtools::load_all() # recompiles the package if necessary
# for some reason, it appears this needs to be run twice otherwise, recent changes to code aren't applied

devtools::load_all()
library(SmokingHistoryGenerator)  # Package name

# helper function
advance_substreams <- function(rng, steps) {
  for (i in 1:steps) {
    rstream.nextsubstream(rng)
  }
}

# eventually, we could probably just send in a single rng into SHG and have it configure the substreams
# for now, this suffices
seed = c(1015873554, 1310354410, 2249465273, 994084013, 2912484720, 3876682925)
base_rng <- new("rstream.mrg32k3a", seed=seed, force.seed=TRUE)

# detectCores() - 2 # Adjust the number of cores as needed
cores <- 10  # Define the number of cores you want to use
N <- 10^6
shg_list <- lapply(1:cores, function(x) {

  # each substream for each shg instance, must be independent
  rng1 <- rstream.clone(base_rng)
  advance_substreams(rng1, (x-1)*4+0)
  rng2 <- rstream.clone(rng1)
  advance_substreams(rng2, (x-1)*4+1)
  rng3 <- rstream.clone(rng2)
  advance_substreams(rng3, (x-1)*4+2)
  rng4 <- rstream.clone(rng3)
  advance_substreams(rng4, (x-1)*4+3)

  shg <- new(SHGInterface)
  shg$initialize()
  shg$setRNGs(rng1, rng2, rng3, rng4)
  shg$setRNGtype("rstream")

  return(shg)
})

start_time <- Sys.time()

num_simulations <- N/cores
result_list <- mclapply(shg_list, function(shg) {
  shg$runSim(num_simulations)
}, mc.cores = cores)

# Combine the results into a single DataFrame
Hx_RSTREAM <- bind_rows(result_list)
end_time <- Sys.time()
end_time - start_time
