library(Rcpp)
library(rbenchmark)
library(rstream)
library(smokingHxGen)
devtools::load_all() # recompiles the package if necessary
devtools::load_all() # recompiles the package if necessary

library(RcppSmokingHistoryGenerator)  # Package name
shg <- new(SHGInterface) # Class

rng1 <- new("rstream.mrg32k3a")
rng2 <- rstream.clone(rng1)
rstream.nextsubstream(rng2)
rng3 <- rstream.clone(rng2)
rstream.nextsubstream(rng3)
rng4 <- rstream.clone(rng3)
rstream.nextsubstream(rng4)
shg$initialize() # instantiates the Smoking_Simulator object in the interface

shg$setRNGtype("MT") # Use internal (non-R) Mersenne Twister
Hx_MT <- shg$runSim(10^6) # should be different (continues with same streams)

shg$setRNGs(rng1, rng2, rng3, rng4) # after initialize so that they can hang off the object
shg$setRNGtype("rstream") # use MRG32k3a with substreams and batch generation
Hx_RSTREAM <- shg$runSim(1000) # should be same as first instance of test

# TODO: investigate the following
# NOTE: the first time the next segment is run, it can produce the following error:
# Error in system.time(replicate(replications, { : external pointer is not valid
# But if you re-run the segment, it appears to run without any errors

# Benchmark the different approaches
N <- 10^6
results <- benchmark(
  SIM_RSTREAM = {
    shg$setRNGtype("rstream")
    RSTREAM_SIM <- shg$runSim(N)
  },
    SIM_MT = {
    shg$setRNGtype("MT")
    MT_SIM <- shg$runSim(N)
  },
  smokingHxGen = {
    pop <-
      list(
        race = rep("white", N),
        sex = sample(x = c("male", "female"), size = N, prob = c(0.5, 0.5), replace = TRUE),
        birth_cohort = rep(1930:1949, N / 20),
        some_other_var = stats::runif(N)
      )

    shg_bladder <- SmokingHistoryGenerator$new(
      population = pop,
      cause_of_death = "all_causes",
      smoking_history_simulation_start_age = getOption("shg.min_smoking_initiation_age"), # 8
      smoking_history_simulation_stop_age = getOption("shg.max_age"), # 110
      lifetime_simulation_spawn_ages = 40,
      lifetime_simulation_stop_ages = getOption("shg.max_age"),
      specifications = getOption("shg.constants"), # coefficients
      validate = TRUE
     )
    SHx <- shg_bladder$get_smoking_exposure_history(copy = FALSE)
  },
  # SHG_CLI = {
  #   curdir = getwd()
  #   setwd("/Users/jclarke/Documents/GitHub/smoking-history-generator/")
  #   system("/Users/jclarke/Documents/GitHub/smoking-history-generator/lbc_smokehist_osx.exe test_input.txt")
  #   setwd(curdir)
  # },
  order = "relative",
  replications = 1 # Number of times to run each expression
)

# Print out the benchmark results
results <- results[, c("test", "elapsed", "relative")]
print(results)

# // Bonus: if we use rstream (MRG32k3a) we can parallelize the simulation (each segment assigned a thread)
# // this is because MRG32k3a allows us to efficiently jump ahead in the stream using substreams; we cannot do that with MT.
# // (so each of the 4 variates would have their own substream for each segment;
# // and for each subsequent segement, we'd use subsequent substreams to ensure independence)
# // eg: [(1,2,3,4), (5,6,7,8), (9,10,11,12), ...]
# // I guess you could also just use a different set of (MT) seeds for each 100K, but no guarantee that the streams are independent

# See https://www.appsilon.com/post/r-doparallel-dataframe
# split by n/(cores-1) and clone + nextsubstream for each run


#N=10^6
#test elapsed relative
#3 smokingHxGen  19.412    1.000
#2       SIM_MT  42.759    2.203
#1  SIM_RSTREAM  45.033    2.320

# 10 cores with RSTREAM
# 7.7 secs

