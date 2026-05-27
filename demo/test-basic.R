library(SmokingHistoryGenerator)

ext2018 <- system.file("extdata", "2018", package = "SmokingHistoryGenerator")
smok_zip <- file.path(ext2018, "bundled-smok.zip")
mort_zip <- file.path(ext2018, "bundled-mort.zip")
if (!nzchar(ext2018) || !file.exists(smok_zip) || !file.exists(mort_zip)) {
  stop("Bundled parameter zips not found under extdata/2018.", call. = FALSE)
}

shg <- new(SHGInterface)
run_cfg <- list(
  smok_params_source = smok_zip,
  mort_params_source = mort_zip,
  mort_params_type = "acm",
  individuals = 10^5,
  race = 0,
  sex = 0,
  cohort_year = 1940
)
shg_apply_config(shg, run_cfg)

bundle <- shg$runSim(run_cfg)
RNGSTREAM_SIM <- bundle$results
