library(SmokingHistoryGenerator)
shg <- new(SHGInterface)

td <- file.path(getwd(), "tests", "testdata")
smok_zip <- normalizePath(file.path(td, "usa-national@smok-NHIS-2018.zip"), mustWork = TRUE)
mort_zip <- normalizePath(file.path(td, "usa-national@mort-v1.0.0.zip"), mustWork = TRUE)

N <- 10^5
race <- 0
sex <- 0
cohort_year <- 1940
run_cfg <- list(
  smok_params_source = smok_zip,
  mort_params_source = mort_zip,
  mort_params_type = "acm",
  individuals = N,
  race = race,
  sex = sex,
  cohort_year = cohort_year
)

shg_apply_config(shg, run_cfg)

bundle <- shg$runSim(run_cfg)
RNGSTREAM_SIM <- bundle$results
