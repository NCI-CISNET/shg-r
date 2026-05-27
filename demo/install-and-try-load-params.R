# Apply bundled parameter zips and run a quick benchmark (wall-clock).
#
# Usage after install:
#   demo(install-and-try-load-params, package = "SmokingHistoryGenerator")
#
# Optional: SHG_DEMO_INDIVIDUALS (default 1e6).

library(SmokingHistoryGenerator)

ext2018 <- system.file("extdata", "2018", package = "SmokingHistoryGenerator")
smok_zip <- file.path(ext2018, "bundled-smok.zip")
mort_zip <- file.path(ext2018, "bundled-mort.zip")
if (!nzchar(ext2018) || !file.exists(smok_zip) || !file.exists(mort_zip)) {
  stop("Bundled parameter zips not found under extdata/2018.", call. = FALSE)
}
message("Smoking zip: ", smok_zip)
message("Mortality zip: ", mort_zip)

n <- as.numeric(Sys.getenv("SHG_DEMO_INDIVIDUALS", "1000000"))
if (!is.finite(n) || n < 1) {
  stop("SHG_DEMO_INDIVIDUALS must be a positive number.", call. = FALSE)
}

shg <- new(SHGInterface)
run_cfg <- list(
  smok_params_source = smok_zip,
  mort_params_source = mort_zip,
  mort_params_type = "ocm",
  individuals = n,
  race = 0,
  sex = 0,
  cohort_year = 2010
)

shg_apply_config(shg, run_cfg)

message("input_data_folder: ", shg$input_data_folder)
message("initiation (relative): ", shg$initiation_filename)
message("initiation (resolved): ",
        file.path(shg$input_data_folder, shg$initiation_filename))

message("Running ", format(n, scientific = FALSE), " individuals...")
t0 <- Sys.time()
bundle <- shg$runSim(run_cfg, attach_run_info = TRUE)
elapsed <- as.numeric(difftime(Sys.time(), t0, units = "secs"))
message("Elapsed (s): ", round(elapsed, 2))
message("Rows: ", nrow(bundle$results))
