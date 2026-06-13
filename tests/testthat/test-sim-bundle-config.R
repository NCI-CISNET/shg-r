library(SmokingHistoryGenerator)
library(testthat)

test_that("runSimFromFixedValues with attach_run_info returns four-slot bundle", {
  shg <- new(SHGInterface)
  shg$input_data_folder <- system.file("extdata", "2018", package = "SmokingHistoryGenerator")

  out <- shg$runSimFromFixedValues(12L, 0L, 0L, 1950L, TRUE, NULL)
  expect_type(out, "list")
  expect_true(all(c("results", "original_config", "repro_config", "run_info") %in% names(out)))
  expect_s3_class(out$results, "data.frame")
  expect_equal(nrow(out$results), 12L)
  expect_equal(out$original_config[["individuals"]], 12L)
  expect_equal(out$original_config[["race"]], 0L)
  expect_equal(out$original_config[["sex"]], 0L)
  expect_equal(out$original_config[["cohort_year"]], 1950L)
  expect_false(identical(out$repro_config, out$run_info))
  expect_true(length(out$run_info$host_platform) > 0L)
  expect_equal(out$run_info$software_versions$shg_core_version, shg$get_shg_core_version())
  expect_true(is.list(out$repro_config$package_repro))
  expect_true("r_package_version" %in% names(out$repro_config$package_repro))
  expect_false("install_fingerprint_md5" %in% names(out$repro_config$package_repro))

  rc <- shg$getReproConfig(FALSE)
  repro_a <- out$repro_config
  repro_b <- rc
  drop <- c("timestamp", "results", "repro_digest")
  repro_a <- repro_a[!names(repro_a) %in% drop]
  repro_b <- repro_b[!names(repro_b) %in% drop]
  expect_identical(repro_a, repro_b)
  expect_true(nzchar(out$repro_config$results$content_md5))
  expect_true(is.list(out$repro_config$results$summary))
  expect_true(nzchar(out$repro_config$repro_digest))
})

test_that("shg_apply_config resets then overlays (sparse cohort_year clears sticky RNG)", {
  shg <- new(SHGInterface)
  shg$input_data_folder <- system.file("extdata", "2018", package = "SmokingHistoryGenerator")

  shg$rng_strategy <- "MersenneTwister"
  shg$reset_seeds_to_defaults()
  expect_equal(shg$rng_strategy, "MersenneTwister")

  shg_apply_config(shg, list(cohort_year = 1950L))
  expect_equal(shg$rng_strategy, "RngStream")
  cy <- shg$getConfig(FALSE)$cohort_year
  expect_true(identical(as.integer(cy)[1], 1950L))
})

test_that("shg_apply_config warns on package_repro identity mismatch", {
  shg <- new(SHGInterface)
  cfg <- list(
    cohort_year = 1950L,
    package_repro = list(
      shg_core_version = "0.0.0",
      install_fingerprint_md5 = "definitely-not-matching"
    )
  )
  expect_warning(
    shg_apply_config(shg, cfg),
    "Config package fingerprint differs"
  )
})

test_that("shg_apply_config warns when saved r_package_version differs", {
  shg <- new(SHGInterface)
  cfg <- list(
    cohort_year = 1950L,
    package_repro = list(r_package_version = "0.0.0-not-a-real-build")
  )
  expect_warning(
    shg_apply_config(shg, cfg),
    "r_package_version"
  )
})

test_that(".shg_results_summary_for_repro omits -999 for means", {
  df <- data.frame(
    smoking_initiation_age = c(-999, 20, -999),
    smoking_cessation_age = c(-999, 25, -999),
    age_at_death = c(80, 70, 75),
    cigarettes_per_day = c(NA_real_, 20, 15)
  )
  s <- SmokingHistoryGenerator:::.shg_results_summary_for_repro(df)
  expect_equal(s$never_smokers$count, 2L)
  expect_equal(s$never_smokers$fraction, 2 / 3)
  expect_equal(s$ever_smokers$cpd_mode, 20L)
  expect_equal(s$ever_smokers$count, 1L)
  expect_equal(s$ever_smokers$fraction, 1 / 3)
  expect_equal(s$smoking_initiation_age$n_obs, 1L)
  expect_equal(s$smoking_initiation_age$mean, 20)
  expect_equal(s$smoking_cessation_age$n_obs, 1L)
  expect_equal(s$smoking_cessation_age$mean, 25)
  expect_equal(s$age_at_death$never_smokers$n_obs, 2L)
  expect_equal(s$age_at_death$never_smokers$mean, mean(c(80, 75)))
  expect_equal(s$age_at_death$ever_smokers$n_obs, 1L)
  expect_equal(s$age_at_death$ever_smokers$mean, 70)
})

test_that(".shg_results_summary_for_repro excludes NA initiation from never and ever", {
  df <- data.frame(
    smoking_initiation_age = c(NA_real_, -999, 18),
    smoking_cessation_age = c(NA_real_, -999, 30),
    age_at_death = c(70, 80, 75),
    cigarettes_per_day = c(NA_real_, NA_real_, 15)
  )
  s <- SmokingHistoryGenerator:::.shg_results_summary_for_repro(df)
  expect_equal(s$n_rows, 3L)
  expect_equal(s$never_smokers$count, 1L)
  expect_equal(s$smoking_initiation_age$n_obs, 1L)
  expect_equal(s$smoking_initiation_age$mean, 18)
})

test_that("shg_run bundle original_config preserves params bundle intent", {
  skip_on_cran()
  z <- shg_test_split_param_zips()
  smok_zip <- z$smok
  mort_zip <- z$mort
  shg <- new(SHGInterface)
  shg$input_data_folder <- system.file("extdata", "2018", package = "SmokingHistoryGenerator")
  cfg <- list(
    smok_params_source = smok_zip,
    mort_params_source = mort_zip,
    mort_params_type = "ocm",
    individuals = 8L,
    race = 0L,
    sex = 0L,
    cohort_year = 1950L
  )
  out <- shg_run(shg, cfg, attach_run_info = TRUE)
  expect_equal(out$original_config$smok_params_source, cfg$smok_params_source)
  expect_equal(out$original_config$mort_params_type, "ocm")
  expect_equal(out$original_config$individuals, 8L)
})

test_that("shg_run with output_file returns bundle and captures output path", {
  shg <- new(SHGInterface)
  shg$input_data_folder <- system.file("extdata", "2018", package = "SmokingHistoryGenerator")
  output_path <- tempfile(fileext = ".csv")
  on.exit(unlink(output_path), add = TRUE)

  cfg <- list(
    output_file = output_path,
    individuals = 12,
    race = 0,
    sex = 0,
    cohort_year = 1950
  )
  out <- shg_run(shg, cfg, attach_run_info = TRUE)
  expect_true(file.exists(output_path))
  expect_true(all(c("results", "original_config", "repro_config", "run_info") %in% names(out)))
  expect_equal(out$original_config$output_file, output_path)
  expect_equal(out$repro_config$output_file, output_path)
  expect_true(grepl("file", out$results$info[1], ignore.case = TRUE))
})

test_that("shg_run auto-applies params when param sources are in config", {
  skip_on_cran()
  z <- shg_test_split_param_zips()
  smok_zip <- z$smok
  mort_zip <- z$mort

  tmp_cache <- tempfile("shg_run_auto_apply_")
  dir.create(tmp_cache)
  old <- Sys.getenv("R_USER_CACHE_DIR", "")
  Sys.setenv(R_USER_CACHE_DIR = tmp_cache)
  on.exit({
    if (nzchar(old)) Sys.setenv(R_USER_CACHE_DIR = old)
    else Sys.unsetenv("R_USER_CACHE_DIR")
    unlink(tmp_cache, recursive = TRUE)
  }, add = TRUE)

  shg <- new(SHGInterface)
  cfg <- list(
    smok_params_source = smok_zip,
    mort_params_source = mort_zip,
    mort_params_type = "ocm",
    individuals = 25,
    cohort_year = 1950
  )
  out <- shg_run(shg, cfg, attach_run_info = TRUE)
  expect_s3_class(out$results, "data.frame")
  expect_equal(nrow(out$results), 25L)
  expect_equal(shg$mortality_filename, "mort/ocm-excl-lung-cancer.csv")
  expect_true(SmokingHistoryGenerator:::.shg_params_paths_exist(shg))
})

test_that("shg_run defaults omitted individuals to 1000", {
  skip_on_cran()
  shg <- new(SHGInterface)
  shg$input_data_folder <- system.file("extdata", "2018", package = "SmokingHistoryGenerator")
  out <- shg_run(shg, list(race = 0L, sex = 0L, cohort_year = 1950L), attach_run_info = FALSE)
  expect_s3_class(out, "data.frame")
  expect_equal(nrow(out), 1000L)
})

test_that("shg_run defaults omitted race and sex to 0", {
  skip_on_cran()
  shg <- new(SHGInterface)
  shg$input_data_folder <- system.file("extdata", "2018", package = "SmokingHistoryGenerator")
  out <- shg_run(shg, list(cohort_year = 1950L), attach_run_info = TRUE)
  expect_equal(out$original_config$cohort_year, 1950L)
  expect_equal(out$repro_config$race, 0L)
  expect_equal(out$repro_config$sex, 0L)
  expect_equal(out$repro_config[["repeat"]], 1000L)
  expect_equal(nrow(out$results), 1000L)
})

test_that("shg_write_config_yaml strips audit keys and writes readable YAML", {
  tf <- tempfile(fileext = ".yml")
  on.exit(unlink(tf), add = TRUE)

  cfg <- list(race = 0, sex = 0, cohort_year = 1940L, run_info = list(dummy = 1L))
  shg_write_config_yaml(cfg, tf)
  rd <- yaml::read_yaml(tf)
  raw <- readLines(tf, warn = FALSE)
  expect_false("run_info" %in% names(rd))
  expect_equal(rd$cohort_year, 1940L)
  expect_true(any(grepl("^race:\\s+0$", raw)))
  expect_true(any(grepl("^sex:\\s+0$", raw)))
  expect_true(any(grepl("^cohort_year:\\s+1940$", raw)))
})
