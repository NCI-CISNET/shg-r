# Legacy mode

In certain settings you might want to configure the generator using a configuration input file (rather than properties) and send the output to a text file. You can do this with the `LegacyRunWebVersion()` method. Sample configuration files live under `tests/testdata/legacy-web-examples/` in the package **source** tree (paths inside them assume you run from the repo root, or rewrite them using `system.file("extdata", package = "SmokingHistoryGenerator")`). Note that if you use `LegacyRunWebVersion()` none of the properties of `shg` you may have set in R will be taken into consideration. Only the properties that you set in the configuration input file will be used. Also note that legacy mode runs with a single segment and with no multi-threading.

```r
library(SmokingHistoryGenerator)
shg <- new(SHGInterface)
wd <- getwd()
setwd(tempdir())
d <- system.file("extdata", package = "SmokingHistoryGenerator")
tf <- tempfile(fileext = ".txt")
writeLines(
  c(
    "RNGSTRATEGY=RngStream",
    "RNGSTREAM_SEED=12345,12345,12345,12345,12345,12345",
    "RACE=0", "SEX=0", "YOB=1950", "CESSATION_YR=0", "REPEAT=100",
    paste0("INIT_PROB=", file.path(d, "smoking", "initiation.csv")),
    paste0("CESS_PROB=", file.path(d, "smoking", "cessation.csv")),
    paste0("MORTALITY_PROB=", file.path(d, "mortality", "acm.csv")),
    paste0("CPD_DATA=", file.path(d, "smoking", "cpd.csv")),
    paste0("OUTPUTFILE=", tempfile("out_", fileext = ".txt")),
    paste0("ERRORFILE=", tempfile("err_", fileext = ".txt"))
  ),
  tf
)
shg$LegacyRunWebVersion(tf)
setwd(wd)
```

From a **git checkout** (package source root as working directory), you can instead run the sample configs under `tests/testdata/legacy-web-examples/`; those files use paths relative to the repo root (`inst/extdata/...`).