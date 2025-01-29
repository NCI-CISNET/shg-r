#install.packages("devtools")
#install.packages("Rcpp")
#install.packages("rstream") # maybe need this later

library(Rcpp)
install.packages("pkgbuild")

library(Rcpp)
#pkgbuild::compile_dll(path=".", debug=FALSE) <-- doesnt seem to remove debug
Sys.setenv(PKG_BUILD_EXTRA_FLAGS = "false")
#pkgbuild::compile_dll(path=".", quiet=FALSE, debug=FALSE)
devtools::load_all()
devtools::load_all()
library(SmokingHistoryGenerator)  # Package name
shg <- new(SHGInterface) # Class

rng1 <- new("rstream.mrg32k3a")
rng2 <- rstream.clone(rng1)
rstream.nextsubstream(rng2)
rng3 <- rstream.clone(rng2)
rstream.nextsubstream(rng3)
rng4 <- rstream.clone(rng3)
rstream.nextsubstream(rng4)

shg$setRNGs(rng1, rng2, rng3, rng4)

# test <- shg$runSim(10)
# test2 <- shg$runSim(10) # should be different (continues with same streams)
# shg$initialize() # resets seeds for all MT streams
# shg$setRNGs(rng1, rng2, rng3, rng4)
#
# test5 <- shg$runSim(10) # should be same as first instance of test (old test for MT only)


# devtools::load_all()
#library(Rcpp)
start_time <- Sys.time()
test <- shg$runSim(10000)
#10K with MRG = 4.26 secs
#10K with MT = 0.426 secs

#41.75??? secs with MT; 4.17 with MRG
end_time <- Sys.time()
end_time - start_time


# Could also load libary and then access member functions
#library(SmokingHistoryGenerator)
#smokinghistorygenerator()

# Also works for
# library(RcppExamples)
# RcppRNGsExample(4)


#https://www.iro.umontreal.ca/~lecuyer/myftp/papers/streams00.pdf
#install.packages("rlecuyer")
#install.packages("rstream")
