library(Rcpp)
library(rbenchmark)
library(rstream)
###################
#TODO NEXT -- replace the other RNGs with the queue method and compare the speed
##################
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
shg$setRNGs(rng1, rng2, rng3, rng4) # after initialize so that they can hang off the object

shg$setRNGtype("MT")
test <- shg$runSim(1000) # should be different (continues with same streams)
shg$setRNGtype("rstream")
test2 <- shg$runSim(1002) # should be same as first instance of test


# Benchmark the different approaches
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

results <- results[, c("test", "elapsed", "relative")]

# Print out the benchmark results
print(results)


# // Bonus: if we use rstream (MRG32k3a) we can parallelize the simulation at this juncture (each segment assigned a thread)
# // this is because MRG32k3a allows us to efficiently jump ahead in the stream using substreams; we cannot do that with MT.
# // (so each of the 4 variates would have their own substream for each segment;
# // and for each subsequent segement, we'd use subsequent substreams to ensure independence)
# // eg: [(1,2,3,4), (5,6,7,8), (9,10,11,12), ...]
# // I guess you could also just use a different set of (MT) seeds for each 100K, but no guarantee that the streams are independent

# See https://www.appsilon.com/post/r-doparallel-dataframe
# split by n/(cores-1) and clone + nextsubstream for each run
