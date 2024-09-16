# I'm not sure why this needs to be in a file in R folder, but for now I'm leaving it here.

#library(Rcpp)
## For R 2.15.1 and later this also works. Note that calling loadModule() triggers
## a load action, so this does not have to be placed in .onLoad() or evalqOnLoad().
loadModule("SmokingSimulator", TRUE)  # Module name

#install.packages("devtools")
#install.packages("Rcpp")

#library(Rcpp)
#devtools::load_all()
#library(RcppSmokingHistoryGenerator)  # Package name
#shg <- new(SHGInterface) # Class
#test = shg$runSim(10)
#test = shg$runSim(10) # should be different
#shg$initialize() # resets seeds
#test = shg$runSim(10) # should be same as first instance of test

