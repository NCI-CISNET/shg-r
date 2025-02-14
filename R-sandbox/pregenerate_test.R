#DID NOT USE THIS, probably can remove

#populate a NumericVector with enough numbers for a simulation

library(Rcpp)
library(rbenchmark)
library(rstream)

devtools::load_all() # recompiles the package if necessary
devtools::load_all() # recompiles the package if necessary

library(SmokingHistoryGenerator)  # Package name
shg <- new(SHGInterface) # Class

rng1 <- new("rstream.mrg32k3a")
rng2 <- rstream.clone(rng1)
rstream.nextsubstream(rng2)
rng3 <- rstream.clone(rng2)
rstream.nextsubstream(rng3)
rng4 <- rstream.clone(rng3)
rstream.nextsubstream(rng4)

rng1_v <- rstream.sample(rng1, 10^7)
rng2_v <- rstream.sample(rng1, 10^7)
rng3_v <- rstream.sample(rng1, 10^7)
rng4_v <- rstream.sample(rng1, 10^7)
