#install.packages("devtools")
#install.packages("Rcpp")
#install.packages("rstream") # maybe need this later

library(Rcpp)
devtools::load_all() # recompiles the package if necessary
library(RcppSmokingHistoryGenerator)  # Package name
shg <- new(SHGInterface) # Class
test <- shg$runSim(10)
test <- shg$runSim(10) # should be different (continues with same streams)
shg$initialize() # resets seeds for all streams
test <- shg$runSim(10) # should be same as first instance of test


# devtools::load_all()
#library(Rcpp)
#start_time <- Sys.time()
#end_time <- Sys.time()
#end_time - start_time


# Could also load libary and then access member functions
#library(RcppSmokingHistoryGenerator)
#smokinghistorygenerator()

# Also works for
library(RcppExamples)
RcppRNGsExample(4)


#https://www.iro.umontreal.ca/~lecuyer/myftp/papers/streams00.pdf
#install.packages("rlecuyer")

#install.packages("rstream")
