# Install SmokingHistoryGenerator from source, apply split parameter bundles,
# and run a quick 1M-individual benchmark (wall-clock).
#
# Usage (from the shg-r repo root):
#   Rscript demo/install-and-try-load-params.R

pkg_root <- normalizePath(".", winslash = "/", mustWork = TRUE)
if (!file.exists(file.path(pkg_root, "DESCRIPTION")))
  stop("Run this from the shg-r package root (where DESCRIPTION lives).")

message("Package root: ", pkg_root)

library(SmokingHistoryGenerator)

td <- file.path(pkg_root, "tests", "testdata")
smok_zip <- normalizePath(file.path(td, "usa-national@smok-NHIS-2018.zip"), mustWork = TRUE)
mort_zip <- normalizePath(file.path(td, "usa-national@mort-v1.0.0.zip"), mustWork = TRUE)
message("Smoking zip: ", smok_zip)
message("Mortality zip: ", mort_zip)

shg <- new(SHGInterface)
run_cfg <- list(
  smok_params_source = smok_zip,
  mort_params_source = mort_zip,
  mort_params_type = "ocm",
  individuals = 1e6,
  race = 0,
  sex = 0,
  cohort_year = 1980
)

shg_apply_config(shg, run_cfg)

message("input_data_folder: ", shg$input_data_folder)
message("initiation (relative): ", shg$initiation_filename)
message("initiation (resolved): ",
        file.path(shg$input_data_folder, shg$initiation_filename))

message("Running 1M individuals...")
t0 <- Sys.time()
bundle <- shg$runSim(run_cfg, attach_run_info = TRUE)
elapsed <- as.numeric(difftime(Sys.time(), t0, units = "secs"))
message("Elapsed (s): ", round(elapsed, 2))
message("Rows: ", nrow(bundle$results))
