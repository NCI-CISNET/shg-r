# Install SmokingHistoryGenerator from source, apply config with params bundle source,
# and run a quick 1M-individual benchmark (wall-clock).
#
# This demo also shows:
# - bundled return with attach_run_info
# - defaults-first apply from sparse config
# - one-writer YAML for sparse vs repro configs
#
# Usage (from the shg-r repo root):
#   Rscript demo/install-and-try-load-params.R
#
# Or in R:  source("demo/install-and-try-load-params.R")

pkg_root <- normalizePath(".", winslash = "/", mustWork = TRUE)
if (!file.exists(file.path(pkg_root, "DESCRIPTION")))
  stop("Run this from the shg-r package root (where DESCRIPTION lives).")

message("Package root: ", pkg_root)

# If needed, install manually before running this demo script.

library(SmokingHistoryGenerator)

zip_path <- normalizePath(
  file.path(pkg_root, "tests", "testdata", "usa-national@smok-2018-mort-2016.zip"),
  winslash = "/",
  mustWork = TRUE
)
message("Zip: ", zip_path)

# Optional HTTP bundle (uncomment to override local path):
# zip_path <- "http://localhost:5173/shg-data/bundles/usa-national@smok-2018-mort-2016.zip"

shg <- new(SHGInterface)
run_cfg <- list(
  params_bundle_source = zip_path,
  mortality = "ocm",
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
message("cessation:  ", shg$cessation_filename)
message("cpd:        ", shg$cpd_filename)
message("mortality (ocm): ", shg$mortality_filename)

message("\n--- benchmark ---")
message(
  "N = ", format(run_cfg$individuals, big.mark = ","),
  "  |  rng_strategy = ", shg$rng_strategy,
  "  |  num_threads = ", shg$num_threads,
  "  |  number_of_segments = ", shg$number_of_segments
)

t0 <- proc.time()
bundle <- shg$runSim(run_cfg, attach_run_info = TRUE)
sim <- bundle$results
elapsed <- proc.time() - t0

message(
  "Elapsed (proc.time): ",
  round(elapsed["elapsed"], 2), " s wall",
  "  (user ", round(elapsed["user.self"], 2), " s, sys ",
  round(elapsed["sys.self"], 2), " s)"
)
message(
  "Throughput: ",
  round(run_cfg$individuals / elapsed["elapsed"]), " individuals / s (wall)"
)
message("Result rows: ", nrow(sim), "  cols: ", ncol(sim))
message("Bundle slots: ", paste(names(bundle), collapse = ", "))
message("run_info keys: ", paste(names(bundle$run_info), collapse = ", "))

intent_yaml <- tempfile("shg-intent-", fileext = ".yml")
repro_yaml <- tempfile("shg-repro-", fileext = ".yml")
shg_write_config_yaml(bundle$original_config, intent_yaml)
shg_write_config_yaml(bundle$repro_config, repro_yaml)
message("Wrote intent YAML: ", intent_yaml)
message("Wrote repro YAML:  ", repro_yaml)

# defaults-first sparse apply example
shg_apply_config(shg, list(cohort_year = 1950))
message("After shg_apply_config(): rng_strategy=", shg$rng_strategy,
        ", cohort_year=", shg$getConfig(FALSE)$cohort_year)

message("\nDone.")
