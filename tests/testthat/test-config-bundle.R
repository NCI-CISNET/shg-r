library(SmokingHistoryGenerator)
library(testthat)

test_that("shg_config_bundle adds NA provenance without load_params", {
  shg <- new(SHGInterface)
  b <- shg_config_bundle(shg)
  expect_type(b, "list")
  expect_false(inherits(b, "shg_config"))
  expect_true(is.na(b$params_bundle_source))
  expect_true(is.na(b$params_mortality))
})

test_that("shg_config_bundle records source after load_params (local zip)", {
  skip_on_cran()
  zip_path <- testthat::test_path("../testdata/usa-national@smok-2016.zip")
  skip_if_not(file.exists(zip_path))

  tmp_cache <- tempfile("shg_cfg_bundle_")
  dir.create(tmp_cache)
  on.exit(unlink(tmp_cache, recursive = TRUE), add = TRUE)
  old <- Sys.getenv("R_USER_CACHE_DIR", "")
  Sys.setenv(R_USER_CACHE_DIR = tmp_cache)
  on.exit({
    if (nzchar(old)) Sys.setenv(R_USER_CACHE_DIR = old)
    else Sys.unsetenv("R_USER_CACHE_DIR")
  }, add = TRUE)

  shg <- new(SHGInterface)
  shg$load_params(url = zip_path, mortality = "ocm")
  b <- shg_config_bundle(shg)
  expect_equal(b$params_bundle_source, zip_path)
  expect_equal(b$params_mortality, "ocm")
  expect_true(nzchar(b$input_data_folder))
})

test_that("shg_use_config_bundle re-extracts when cache folder exists but files gone", {
  skip_on_cran()
  zip_path <- testthat::test_path("../testdata/usa-national@smok-2016.zip")
  skip_if_not(file.exists(zip_path))

  tmp_cache <- tempfile("shg_cfg_rehydr_")
  dir.create(tmp_cache)
  on.exit(unlink(tmp_cache, recursive = TRUE), add = TRUE)
  old <- Sys.getenv("R_USER_CACHE_DIR", "")
  Sys.setenv(R_USER_CACHE_DIR = tmp_cache)
  on.exit({
    if (nzchar(old)) Sys.setenv(R_USER_CACHE_DIR = old)
    else Sys.unsetenv("R_USER_CACHE_DIR")
  }, add = TRUE)

  yml <- tempfile(fileext = ".yml")
  on.exit(unlink(yml), add = TRUE)

  shg1 <- new(SHGInterface)
  shg1$load_params(url = zip_path)
  shg1$runSimFromFixedValues(50, 0, 0, 1950)
  shg_save_config(shg1, yml, quiet = TRUE)

  unlink(shg1$input_data_folder, recursive = TRUE)

  shg2 <- new(SHGInterface)
  expect_message(
    shg_use_config_bundle(shg2, yml),
    "re-loading bundle"
  )
  expect_true(file.exists(file.path(shg2$input_data_folder, shg2$initiation_filename)))
  expect_equal(shg2$mortality_filename, shg1$mortality_filename)
})

test_that("YAML config: cache reuse, clear cache, load_config re-fetches", {
  skip_on_cran()
  zip_path <- testthat::test_path("../testdata/usa-national@smok-2016.zip")
  skip_if_not(file.exists(zip_path))

  tmp_cache <- tempfile("shg_cfg_e2e_")
  dir.create(tmp_cache)
  old <- Sys.getenv("R_USER_CACHE_DIR", "")
  Sys.setenv(R_USER_CACHE_DIR = tmp_cache)
  on.exit({
    if (nzchar(old)) Sys.setenv(R_USER_CACHE_DIR = old)
    else Sys.unsetenv("R_USER_CACHE_DIR")
    unlink(tmp_cache, recursive = TRUE)
  }, add = TRUE)

  yml <- tempfile(fileext = ".yml")
  on.exit(unlink(yml), add = TRUE)

  shg1 <- new(SHGInterface)
  shg1$load_params(url = zip_path, mortality = "acm")
  shg1$runSimFromFixedValues(100, 0, 0, 1950)
  shg_save_config(shg1, yml, quiet = TRUE)

  shg2 <- new(SHGInterface)
  cfg2 <- shg_load_config(shg2, yml)
  expect_type(cfg2, "list")
  expect_true(file.exists(file.path(shg2$input_data_folder, shg2$initiation_filename)))
  expect_equal(shg2$mortality_filename, shg1$mortality_filename)
  expect_equal(shg2$params_bundle_source, zip_path)

  shg3 <- new(SHGInterface)
  expect_message(
    shg_load_params(shg3, url = zip_path),
    "Using cached"
  )
  expect_true(file.exists(file.path(shg3$input_data_folder, shg3$initiation_filename)))

  shg_clear_params_cache()

  shg4 <- new(SHGInterface)
  expect_message(
    shg_load_config(shg4, yml),
    "re-loading bundle"
  )
  expect_true(file.exists(file.path(shg4$input_data_folder, shg4$initiation_filename)))
  expect_equal(shg4$mortality_filename, shg1$mortality_filename)
})

test_that("shg_load_config restores config and can re-fetch after cache clear", {
  skip_on_cran()
  zip_path <- testthat::test_path("../testdata/usa-national@smok-2016.zip")
  skip_if_not(file.exists(zip_path))

  tmp_cache <- tempfile("shg_cfg_load_params_config_")
  dir.create(tmp_cache)
  old <- Sys.getenv("R_USER_CACHE_DIR", "")
  Sys.setenv(R_USER_CACHE_DIR = tmp_cache)
  on.exit({
    if (nzchar(old)) Sys.setenv(R_USER_CACHE_DIR = old)
    else Sys.unsetenv("R_USER_CACHE_DIR")
    unlink(tmp_cache, recursive = TRUE)
  }, add = TRUE)

  yml <- tempfile(fileext = ".yml")
  on.exit(unlink(yml), add = TRUE)

  shg1 <- new(SHGInterface)
  shg1$load_params(url = zip_path, mortality = "ocm")
  shg1$runSimFromFixedValues(80, 0, 0, 1950)
  shg_save_config(shg1, yml, quiet = TRUE)

  shg_clear_params_cache()

  shg2 <- new(SHGInterface)
  expect_message(
    shg_load_config(shg2, yml),
    "re-loading bundle"
  )
  expect_true(file.exists(file.path(shg2$input_data_folder, shg2$initiation_filename)))
  expect_equal(shg2$mortality_filename, "mortality/ocm-excl-lung-cancer.csv")
})

test_that("SHGInterface$load_config and runSim delegate correctly", {
  skip_on_cran()
  zip_path <- testthat::test_path("../testdata/usa-national@smok-2016.zip")
  skip_if_not(file.exists(zip_path))

  tmp_cache <- tempfile("shg_cfg_load_config_method_")
  dir.create(tmp_cache)
  old <- Sys.getenv("R_USER_CACHE_DIR", "")
  Sys.setenv(R_USER_CACHE_DIR = tmp_cache)
  on.exit({
    if (nzchar(old)) Sys.setenv(R_USER_CACHE_DIR = old)
    else Sys.unsetenv("R_USER_CACHE_DIR")
    unlink(tmp_cache, recursive = TRUE)
  }, add = TRUE)

  yml <- tempfile(fileext = ".yml")
  on.exit(unlink(yml), add = TRUE)

  shg1 <- new(SHGInterface)
  shg1$load_params(url = zip_path)
  shg1$runSimFromFixedValues(40, 0, 0, 1950)
  shg_save_config(shg1, yml, quiet = TRUE)

  shg_clear_params_cache()

  shg2 <- new(SHGInterface)
  cfg <- NULL
  expect_message({ cfg <- shg2$load_config(yml) }, "re-loading bundle")
  expect_true(is.list(cfg))
  expect_true(file.exists(file.path(shg2$input_data_folder, shg2$initiation_filename)))

  out <- shg2$runSim(cfg)
  expect_s3_class(out, "data.frame")
  expect_equal(nrow(out), 40)

  out2 <- shg_run(shg2, cfg)
  expect_equal(nrow(out2), 40)
})

test_that("SHGInterface$save_config matches shg_save_config output", {
  skip_on_cran()
  zip_path <- testthat::test_path("../testdata/usa-national@smok-2016.zip")
  skip_if_not(file.exists(zip_path))

  tmp_cache <- tempfile("shg_cfg_save_method_")
  dir.create(tmp_cache)
  old <- Sys.getenv("R_USER_CACHE_DIR", "")
  Sys.setenv(R_USER_CACHE_DIR = tmp_cache)
  on.exit({
    if (nzchar(old)) Sys.setenv(R_USER_CACHE_DIR = old)
    else Sys.unsetenv("R_USER_CACHE_DIR")
    unlink(tmp_cache, recursive = TRUE)
  }, add = TRUE)

  yml1 <- tempfile(fileext = ".yml")
  yml2 <- tempfile(fileext = ".yml")
  on.exit(unlink(c(yml1, yml2)), add = TRUE)

  shg <- new(SHGInterface)
  shg$load_params(url = zip_path)
  shg$runSimFromFixedValues(10, 0, 0, 1950)
  shg$save_config(yml1, quiet = TRUE)
  shg_save_config(shg, yml2, quiet = TRUE)

  expect_equal(readLines(yml1), readLines(yml2))
})

test_that("shg_save_config errors after population run following fixed cohort run", {
  skip_on_cran()
  zip_path <- testthat::test_path("../testdata/usa-national@smok-2016.zip")
  skip_if_not(file.exists(zip_path))

  tmp_cache <- tempfile("shg_cfg_save_pop_invalidate_")
  dir.create(tmp_cache)
  old <- Sys.getenv("R_USER_CACHE_DIR", "")
  Sys.setenv(R_USER_CACHE_DIR = tmp_cache)
  on.exit({
    if (nzchar(old)) Sys.setenv(R_USER_CACHE_DIR = old)
    else Sys.unsetenv("R_USER_CACHE_DIR")
    unlink(tmp_cache, recursive = TRUE)
  }, add = TRUE)

  yml <- tempfile(fileext = ".yml")
  on.exit(unlink(yml), add = TRUE)

  shg <- new(SHGInterface)
  shg$num_threads <- 1
  shg$number_of_segments <- 1
  shg$load_params(url = zip_path)
  shg$runSimFromFixedValues(5, 0, 0, 1950)
  pop <- data.frame(
    race = rep(0, 3),
    sex = rep(0, 3),
    birth_cohort = rep(1950, 3)
  )
  shg$runSimFromDataFrame(pop)
  expect_error(shg_save_config(shg, yml, quiet = TRUE), "runSimFromDataFrame")
})

test_that("shg_save_config errors when run metadata not recorded", {
  skip_on_cran()
  zip_path <- testthat::test_path("../testdata/usa-national@smok-2016.zip")
  skip_if_not(file.exists(zip_path))

  tmp_cache <- tempfile("shg_cfg_save_fail_")
  dir.create(tmp_cache)
  old <- Sys.getenv("R_USER_CACHE_DIR", "")
  Sys.setenv(R_USER_CACHE_DIR = tmp_cache)
  on.exit({
    if (nzchar(old)) Sys.setenv(R_USER_CACHE_DIR = old)
    else Sys.unsetenv("R_USER_CACHE_DIR")
    unlink(tmp_cache, recursive = TRUE)
  }, add = TRUE)

  shg <- new(SHGInterface)
  shg$load_params(url = zip_path)
  yml <- tempfile(fileext = ".yml")
  on.exit(unlink(yml), add = TRUE)
  expect_error(shg_save_config(shg, yml, quiet = TRUE), "runSimFromFixedValues")
})
