library(SmokingHistoryGenerator)
library(testthat)

test_that("runSimFromFixedValues with attach_run_info returns four-slot bundle", {
  shg <- new(SHGInterface)
  shg$input_data_folder <- system.file("extdata", package = "SmokingHistoryGenerator")

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
  expect_true("install_fingerprint_md5" %in% names(out$repro_config$package_repro))

  rc <- shg$getReproConfig(FALSE)
  repro_a <- out$repro_config
  repro_b <- rc
  repro_a$timestamp <- NULL
  repro_b$timestamp <- NULL
  expect_identical(repro_a, repro_b)
})

test_that("shg_apply_config resets then overlays (sparse cohort_year clears sticky RNG)", {
  shg <- new(SHGInterface)
  shg$input_data_folder <- system.file("extdata", package = "SmokingHistoryGenerator")

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

test_that("shg_run bundle original_config preserves params bundle intent", {
  shg <- new(SHGInterface)
  shg$input_data_folder <- system.file("extdata", package = "SmokingHistoryGenerator")
  cfg <- list(
    params_bundle_source = "https://example.invalid/params.zip",
    params_mortality = "ocm",
    individuals = 8L,
    race = 0L,
    sex = 0L,
    cohort_year = 1950L
  )
  out <- shg_run(shg, cfg, attach_run_info = TRUE)
  expect_equal(out$original_config$params_bundle_source, cfg$params_bundle_source)
  expect_equal(out$original_config$params_mortality, "ocm")
  expect_equal(out$original_config$individuals, 8L)
})

test_that("shg_run defaults omitted individuals to 1000", {
  skip_on_cran()
  shg <- new(SHGInterface)
  shg$input_data_folder <- system.file("extdata", package = "SmokingHistoryGenerator")
  out <- shg_run(shg, list(race = 0L, sex = 0L, cohort_year = 1950L), attach_run_info = FALSE)
  expect_s3_class(out, "data.frame")
  expect_equal(nrow(out), 1000L)
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
