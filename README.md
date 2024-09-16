# RcppSmokingHistoryGenerator

## Installation for developers
Retrieve the  rcpp-shg` repo and open an R session in R (or R Studio)
Dependencies:
```r
install.packages("devtools")
install.packages("Rcpp")
```

Ensure Rcpp is loaded
```r
library(Rcpp)
```

Then initially and each time you make changes to the src directory
```r

devtools::load_all() # recompiles the package if necessary
library(RcppSmokingHistoryGenerator)  # Package name
```

# Example usage
```r
shg <- new(SHGInterface) # Class
test <- shg$runSim(10)
test <- shg$runSim(10) # should be different (continues with same streams)
shg$initialize() # resets seeds for all streams
test <- shg$runSim(10) # should be same as first instance of test
```