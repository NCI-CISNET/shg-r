# Portable YAML reproducibility demo
#
# Run with:
#   demo("portable-yaml-workflow", package = "SmokingHistoryGenerator")
#
# This demonstrates scientific reproducibility with a portable config:
# 1) load params from a zip bundle
# 2) run a fixed cohort simulation
# 3) save portable YAML
# 4) clear cache, reload from YAML, rerun, compare

library(SmokingHistoryGenerator)

resolve_demo_zip <- function() {
  # 1) allow explicit override (local path or URL)
  z <- Sys.getenv("SHG_PARAMS_ZIP", "")
  if (nzchar(z)) return(z)

  # 2) local repo checkout path (works for developers)
  candidate <- normalizePath(
    file.path(getwd(), "tests", "testdata", "usa-national@smok-2016.zip"),
    winslash = "/",
    mustWork = FALSE
  )
  if (file.exists(candidate)) return(candidate)

  stop(
    "No parameter bundle configured for demo.\n",
    "Set SHG_PARAMS_ZIP to a local bundle path or URL, e.g.:\n",
    "  Sys.setenv(SHG_PARAMS_ZIP = '/path/to/usa-national@smok-2016.zip')\n",
    "then re-run demo('portable-yaml-workflow', package='SmokingHistoryGenerator').",
    call. = FALSE
  )
}

zip_path <- resolve_demo_zip()
config_path <- tempfile("shg-portable-config-", fileext = ".yml")

cat("Bundle source:", zip_path, "\n")
cat("Portable YAML:", config_path, "\n")

shg <- new(SHGInterface)
shg$load_params(url = zip_path, mortality = "ocm")

shape <- shg_params_summary(shg)
fmt_range_count <- function(x) {
  if (is.null(x) || length(x) == 0L) return("[NA, NA], n=0")
  paste0("[", x[["min"]], ", ", x[["max"]], "], n=", x[["count"]])
}
cat("\n--- Parameter shape summary ---\n")
cat("Counts: races=", shape$num_races, ", sexes=", shape$num_sexes,
    ", cohorts=", shape$num_cohorts, "\n", sep = "")
cat("Initiation cohorts: ", fmt_range_count(shape$initiation$cohorts), "\n", sep = "")
cat("Cessation cohorts:  ", fmt_range_count(shape$cessation$cohorts), "\n", sep = "")
cat("Mortality cohorts:  ", fmt_range_count(shape$mortality$cohorts), "\n", sep = "")
cat("CPD cohorts:        ", fmt_range_count(shape$cpd$cohorts), "\n", sep = "")
cat("CPD note:", shape$cpd$note, "\n")

N <- 1e5
race <- 0
sex <- 0
cohort_year <- 2010
sim1 <- shg$runSimFromFixedValues(N, race, sex, cohort_year)

stopifnot(isTRUE(shg$last_completed_sim_was_fixed_cohort()))
shg$save_config(config_path)

cat("\nSaved portable YAML from last fixed cohort run.\n")
cat("Clearing parameter cache and restoring from YAML...\n")
shg_clear_params_cache()

shg2 <- new(SHGInterface)
config <- shg2$load_config(config_path)
sim2 <- shg2$runSim(config)

identical_results <- isTRUE(all.equal(sim1, sim2))
cat("Results identical:", identical_results, "\n")
if (!identical_results) {
  cat("Differences detected.\n")
  print(summary(abs(sim1 - sim2)))
} else {
  cat("Success: platform-agnostic reproducibility check passed.\n")
}

cat("\n--- Portable YAML on disk ---\n")
cat(readLines(config_path), sep = "\n")
cat("\n")
