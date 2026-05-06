# Developer notes

If you want to contribute to the SHG R wrapper and/or compile the package from source you must first retrieve the `shg-r` repository from Github and open an R session.
```r
setwd("path-to-shg-r")
install.packages("devtools")
Sys.setenv(PKG_BUILD_EXTRA_FLAGS = "false") # optional, but should increase performance
devtools::load_all()
```

For GitHub users/devs who want to (re)install the local checkout into their library,
you can use `pak` directly from the repo root:

```r
install.packages("pak")
setwd("path-to-shg-r")
pak::pak(".")
```

After install, start a **new R session** before reloading/testing to avoid stale package
state from a previously loaded DLL.

Then initially and each time you make changes to the src directory:
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

## Performance optimization (developer/local builds)

The package is built with `-O3` by default. For machine-specific speedups on local runs,
you can enable CPU-targeted instructions in `~/.R/Makevars`:

```makefile
CXX17FLAGS += -march=native
```

This can improve throughput for numeric code, but binaries built with `-march=native`
are not portable across different CPU families.
