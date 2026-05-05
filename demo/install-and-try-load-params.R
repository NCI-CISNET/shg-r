# Install SmokingHistoryGenerator from source, load_params from the test zip,
# and run a quick 1M-individual benchmark (wall-clock).
#
# Usage (from the shg-r repo root):
#   Rscript install-and-try-load-params.R
#
# Or in R:  source("install-and-try-load-params.R")

stopifnot(requireNamespace("devtools", quietly = TRUE))

pkg_root <- normalizePath(".", winslash = "/", mustWork = TRUE)
if (!file.exists(file.path(pkg_root, "DESCRIPTION")))
  stop("Run this from the shg-r package root (where DESCRIPTION lives).")

message("Package root: ", pkg_root)

#devtools::install(pkg_root, quiet = FALSE, upgrade = FALSE)

library(SmokingHistoryGenerator)

zip_path <- normalizePath(
  file.path(pkg_root, "tests", "testdata", "usa-national@smok-2016.zip"),
  winslash = "/",
  mustWork = TRUE
)
message("Zip: ", zip_path)

shg <- new(SHGInterface)
shg$load_params(url = zip_path)

message("input_data_folder: ", shg$input_data_folder)
message("initiation (relative): ", shg$initiation_filename)
message("initiation (resolved): ",
        file.path(shg$input_data_folder, shg$initiation_filename))
message("cessation:  ", shg$cessation_filename)
message("cpd:        ", shg$cpd_filename)
message("mortality:  ", shg$mortality_filename)

shg$load_params(url = zip_path, mortality = "ocm")
message("mortality (ocm): ", shg$mortality_filename)

# --- benchmark: 1M individuals (single cohort cell: race, sex, cohort_year) ---
N <- 1e6
race <- 0
sex <- 0
cohort_year <- 1980

message("\n--- benchmark ---")
message(
  "N = ", format(N, big.mark = ","),
  "  |  rng_strategy = ", shg$rng_strategy,
  "  |  num_threads = ", shg$num_threads,
  "  |  number_of_segments = ", shg$number_of_segments
)

t0 <- proc.time()
sim <- shg$runSimFromFixedValues(N, race, sex, cohort_year)
elapsed <- proc.time() - t0

message(
  "Elapsed (proc.time): ",
  round(elapsed["elapsed"], 2), " s wall",
  "  (user ", round(elapsed["user.self"], 2), " s, sys ",
  round(elapsed["sys.self"], 2), " s)"
)
message(
  "Throughput: ",
  round(N / elapsed["elapsed"]), " individuals / s (wall)"
)
message("Result rows: ", nrow(sim), "  cols: ", ncol(sim))

message("\nConfig snapshot (includes zip provenance if present):")
str(shg_config_bundle(shg), max.level = 2, list.len = 99)

message("\nDone.")
