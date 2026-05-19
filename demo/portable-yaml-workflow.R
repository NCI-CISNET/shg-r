# Portable YAML reproducibility demo
#
# Run with:
#   demo("portable-yaml-workflow", package = "SmokingHistoryGenerator")
#
# This demonstrates scientific reproducibility with a portable config:
# 1) apply config with params_bundle_source (which restores bundle tables)
# 2) run a fixed cohort simulation
# 3) collect bundled outputs (results + original_config + repro_config + run_info)
# 4) write sparse + repro YAML with one writer
# 5) clear cache, reload from YAML, rerun, compare

library(SmokingHistoryGenerator)

resolve_demo_zip <- function() {
  # 1) allow explicit override (local path or URL)
  z <- Sys.getenv("SHG_PARAMS_ZIP", "")
  if (nzchar(z)) return(z)

  # 2) local repo checkout path (works for developers)
  candidate <- normalizePath(
    file.path(getwd(), "tests", "testdata", "usa-national@smok-2018-mort-2016.zip"),
    winslash = "/",
    mustWork = FALSE
  )
  if (file.exists(candidate)) return(candidate)

  stop(
    "No parameter bundle configured for demo.\n",
    "Set SHG_PARAMS_ZIP to a local bundle path or URL, e.g.:\n",
    "  Sys.setenv(SHG_PARAMS_ZIP = '/path/to/usa-national@smok-2018-mort-2016.zip')\n",
    "then re-run demo('portable-yaml-workflow', package='SmokingHistoryGenerator').",
    call. = FALSE
  )
}

zip_path <- resolve_demo_zip()
config_path <- tempfile("shg-portable-config-", fileext = ".yml")

cat("Bundle source:", zip_path, "\n")
cat("Portable YAML:", config_path, "\n")

shg <- new(SHGInterface)
run_cfg <- list(
  params_bundle_source = zip_path,
  params_mortality = "ocm",
  individuals = 100000,
  race = 0,
  sex = 0,
  cohort_year = 2010
)
shg_apply_config(shg, run_cfg)

shape <- shg_params_summary(shg)
fmt_range_count <- function(x) {
  if (is.null(x) || length(x) == 0) return("[NA, NA], n=0")
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

bundle1 <- shg$runSim(run_cfg)
sim1 <- bundle1$results

stopifnot(isTRUE(shg$last_completed_sim_was_fixed_cohort()))
shg$save_config(config_path)

intent_path <- tempfile("shg-intent-config-", fileext = ".yml")
repro_path <- tempfile("shg-repro-config-", fileext = ".yml")
shg_write_config_yaml(bundle1$original_config, intent_path)
shg_write_config_yaml(bundle1$repro_config, repro_path)

cat("\nSaved portable YAML from last fixed cohort run.\n")
cat("Saved sparse intent YAML:", intent_path, "\n")
cat("Saved repro YAML:", repro_path, "\n")
cat("Bundle run_info keys:", paste(names(bundle1$run_info), collapse = ", "), "\n")
cat("Clearing parameter cache and restoring from YAML...\n")
shg_clear_params_cache()

shg2 <- new(SHGInterface)
config <- shg2$load_config(config_path)
sim2 <- shg2$runSim(config)
sim2 <- sim2$results

comparison <- all.equal(sim1, sim2)
identical_results <- isTRUE(comparison)
cat("Results identical:", identical_results, "\n")
if (!identical_results) {
  cat("Differences detected.\n")
  print(comparison)
} else {
  cat("Success: platform-agnostic reproducibility check passed.\n")
}

cat("\n--- Portable YAML on disk ---\n")
cat(readLines(config_path), sep = "\n")
cat("\n")
