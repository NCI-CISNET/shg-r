# Developer notes

If you want to contribute to the SHG R wrapper and/or compile the package from source you must first retrieve the `shg-rcpp` repository from Github and open an R session.
```r
setwd("path-to-shg-rcpp")
install.packages("devtools")
Sys.setenv(PKG_BUILD_EXTRA_FLAGS = "false") # optional, but should increase performance
devtools::load_all()
```

Then initially and each time you make changes to the src directory
```r
# If you want to prevent the pedantic and -O0 optimization flags (slower)
Sys.setenv(PKG_BUILD_EXTRA_FLAGS = "false")

# If you want to force a recompile
devtools::clean_dll()

# Note: debug = TRUE results in "(debug build)" and -O0 -g etc. which overrides Makevars
pkgbuild::compile_dll(path = ".", debug = FALSE)

# Recompile the package if necessary (typically after changes to the C++ source)
devtools::load_all() 
library(SmokingHistoryGenerator)
```
