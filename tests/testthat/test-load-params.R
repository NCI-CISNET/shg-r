test_that("shg_params_cache_dir() returns a character path", {
  d <- shg_params_cache_dir()
  expect_type(d, "character")
  expect_true(nzchar(d))
})

test_that("SHGInterface params_cache_dir matches shg_params_cache_dir()", {
  shg <- new(SHGInterface)
  expect_equal(shg$params_cache_dir, shg_params_cache_dir())
})

test_that("shg_params_summary returns parameter shape from configured files", {
  shg <- new(SHGInterface)
  d <- system.file("extdata", "2018", package = "SmokingHistoryGenerator")
  shg$input_data_folder <- d

  s <- shg_params_summary(shg)
  expect_type(s, "list")
  expect_true(all(c("num_races", "num_sexes", "num_cohorts") %in% names(s)))
  expect_true(all(c("cohort_start_years", "cohort_end_years") %in% names(s)))
  expect_true(all(c("initiation", "cessation", "mortality", "cpd") %in% names(s)))
  expect_equal(length(s$cohort_start_years), s$num_cohorts)
  expect_equal(length(s$cohort_end_years), s$num_cohorts)
  expect_true(s$num_races >= 1)
  expect_true(s$num_sexes >= 1)
  expect_true(all(c("ages", "num_races", "num_sexes", "cohorts") %in% names(s$initiation)))
  expect_true(all(c("ages", "num_races", "num_sexes", "cohorts") %in% names(s$cessation)))
  expect_true(all(c("ages", "years", "num_races", "num_sexes", "cohorts") %in% names(s$mortality)))
  expect_true(all(c("ages", "cohorts", "num_races", "num_sexes",
                    "num_intensity_groups", "rows_loaded", "rows_skipped", "note") %in% names(s$cpd)))
  expect_true(all(c("min", "max", "count", "values") %in% names(s$initiation$cohorts)))
  expect_true(all(c("min", "max", "count", "values") %in% names(s$cessation$cohorts)))
  expect_true(all(c("min", "max", "count", "values") %in% names(s$mortality$cohorts)))
  expect_true(all(c("min", "max", "count", "values", "windows") %in% names(s$cpd$cohorts)))
  expect_equal(s$initiation$cohorts$count, s$num_cohorts)
  expect_equal(s$cessation$cohorts$count, s$num_cohorts)
  expect_true(s$cpd$cohorts$count >= 1)
  expect_type(s$cpd$note, "character")
  expect_true(length(s$cpd$note) == 1 && nzchar(s$cpd$note))
  expect_true(is.list(s$cpd$initiation_alignment))
  expect_match(s$cpd$note, "ages 0-7")
  expect_match(s$cpd$note, "effectively ignored")
})

test_that(".shg_cpd_initiation_note handles dot rows as ignorable", {
  f <- SmokingHistoryGenerator:::.shg_cpd_initiation_note
  tmp <- tempfile(fileext = ".csv")
  on.exit(unlink(tmp), add = TRUE)

  dat <- data.frame(
    RACE = c(0, 0, 0),
    SEX = c(0, 0, 0),
    AGE = c(0, 1, 8),
    `1950` = c(".", "0", "0.1"),
    check.names = FALSE
  )
  utils::write.csv(dat, tmp, row.names = FALSE, quote = FALSE)

  out <- f(tmp, 8)
  expect_equal(out$details$status, "ok")
  expect_match(out$note, "0 or '\\.'")
  expect_match(out$note, "treated as missing")
})

test_that(".shg_cpd_initiation_note flags non-zero initiation below cpd min age", {
  f <- SmokingHistoryGenerator:::.shg_cpd_initiation_note
  tmp <- tempfile(fileext = ".csv")
  on.exit(unlink(tmp), add = TRUE)

  dat <- data.frame(
    RACE = c(0, 0, 0),
    SEX = c(0, 0, 0),
    AGE = c(0, 1, 8),
    `1950` = c("0.05", "0", "0.1"),
    check.names = FALSE
  )
  utils::write.csv(dat, tmp, row.names = FALSE, quote = FALSE)

  out <- f(tmp, 8)
  expect_equal(out$details$status, "needs-review")
  expect_match(out$note, "non-zero initiation")
})

test_that(".shg_assert_downloaded_zip rejects HTML-like content", {
  tmp <- tempfile(fileext = ".zip")
  on.exit(unlink(tmp), add = TRUE)
  writeBin(charToRaw("<html><head><title>404</title></html>"), tmp)
  expect_error(
    SmokingHistoryGenerator:::.shg_assert_downloaded_zip(tmp, "https://example.com/bad.zip"),
    "HTML"
  )
})

test_that(".shg_assert_downloaded_zip rejects empty file", {
  tmp <- tempfile(fileext = ".zip")
  on.exit(unlink(tmp), add = TRUE)
  file.create(tmp)
  expect_error(
    SmokingHistoryGenerator:::.shg_assert_downloaded_zip(tmp, "https://example.com/x.zip"),
    "empty"
  )
})

test_that(".shg_assert_downloaded_zip accepts a real parameter zip", {
  z <- shg_test_split_param_zips()
  expect_identical(
    SmokingHistoryGenerator:::.shg_assert_downloaded_zip(z$smok, z$smok),
    z$smok
  )
})

test_that(".shg_download_options reads package options", {
  old <- options(shg.params.download.timeout_sec = 123, shg.params.download.connect_sec = 45)
  on.exit(options(old), add = TRUE)
  o <- SmokingHistoryGenerator:::.shg_download_options()
  expect_equal(o$timeout_sec, 123)
  expect_equal(o$connect_sec, 45)
})

test_that(".shg_url_cache_key produces stable key", {
  f <- SmokingHistoryGenerator:::.shg_url_cache_key
  k1 <- f("https://example.com/snap.zip")
  k2 <- f("https://example.com/snap.zip")
  expect_equal(k1, k2)
  expect_match(k1, "^snap_[0-9a-f]{8}$")
})

test_that(".shg_url_cache_key differs for different URLs", {
  f <- SmokingHistoryGenerator:::.shg_url_cache_key
  expect_false(f("https://a.com/x.zip") == f("https://b.com/x.zip"))
})

test_that("shg_clear_params_cache: no-op when cache does not exist", {
  # Temporarily redirect cache to nonexistent dir
  old <- Sys.getenv("R_USER_CACHE_DIR", "")
  tmp <- tempfile()
  Sys.setenv(R_USER_CACHE_DIR = tmp)
  on.exit({
    if (nzchar(old)) Sys.setenv(R_USER_CACHE_DIR = old)
    else Sys.unsetenv("R_USER_CACHE_DIR")
  }, add = TRUE)

  expect_message(shg_clear_params_cache(), "does not exist")
})

test_that("shg_load_params configures SHGInterface from local extracted dir", {
  skip_if_not(requireNamespace("SmokingHistoryGenerator", quietly = TRUE))

  # Build a minimal fake param bundle directory
  tmp_root <- tempfile("shg_params_test_")
  smk_dir  <- file.path(tmp_root, "smoking")
  mort_dir  <- file.path(tmp_root, "mortality")
  dir.create(smk_dir, recursive = TRUE)
  dir.create(mort_dir, recursive = TRUE)
  file.create(file.path(smk_dir, "initiation.csv"))
  file.create(file.path(smk_dir, "cessation.csv"))
  file.create(file.path(smk_dir, "cpd.csv"))
  file.create(file.path(mort_dir, "acm.csv"))
  file.create(file.path(mort_dir, "ocm-excl-lung-cancer.csv"))
  on.exit(unlink(tmp_root, recursive = TRUE), add = TRUE)

  shg <- new(SHGInterface)
  # Call the internal apply directly (bypasses download)
  SmokingHistoryGenerator:::.shg_apply_params(shg, tmp_root, "acm")

  expect_equal(shg$input_data_folder, tmp_root)
  expect_equal(shg$initiation_filename, "smoking/initiation.csv")
  expect_equal(shg$cessation_filename, "smoking/cessation.csv")
  expect_equal(shg$cpd_filename, "smoking/cpd.csv")
  expect_equal(shg$mortality_filename, "mortality/acm.csv")
  expect_true(file.exists(file.path(shg$input_data_folder, shg$initiation_filename)))
})

test_that("shg_load_params ocm mortality sets ocm filename", {
  tmp_root <- tempfile("shg_params_ocm_")
  smk_dir  <- file.path(tmp_root, "smoking")
  mort_dir  <- file.path(tmp_root, "mortality")
  dir.create(smk_dir, recursive = TRUE)
  dir.create(mort_dir, recursive = TRUE)
  file.create(file.path(smk_dir, "initiation.csv"))
  file.create(file.path(smk_dir, "cessation.csv"))
  file.create(file.path(smk_dir, "cpd.csv"))
  file.create(file.path(mort_dir, "acm.csv"))
  file.create(file.path(mort_dir, "ocm-excl-lung-cancer.csv"))
  on.exit(unlink(tmp_root, recursive = TRUE), add = TRUE)

  shg <- new(SHGInterface)
  SmokingHistoryGenerator:::.shg_apply_params(shg, tmp_root, "ocm")

  expect_true(grepl("ocm-excl-lung-cancer", shg$mortality_filename))
})

test_that("shg_load_params errors on missing smoking files", {
  tmp_root <- tempfile("shg_params_bad_")
  dir.create(file.path(tmp_root, "smoking"), recursive = TRUE)
  dir.create(file.path(tmp_root, "mortality"), recursive = TRUE)
  # Only create one file — missing cessation.csv and cpd.csv
  file.create(file.path(tmp_root, "smoking", "initiation.csv"))
  on.exit(unlink(tmp_root, recursive = TRUE), add = TRUE)

  shg <- new(SHGInterface)
  expect_error(
    SmokingHistoryGenerator:::.shg_apply_params(shg, tmp_root, "acm"),
    "missing expected files|incomplete"
  )
})

test_that("load_params method exists on SHGInterface", {
  shg <- new(SHGInterface)
  expect_true(is.function(shg$load_params))
})

test_that(".shg_snapshot_root detects nested layout", {
  f    <- SmokingHistoryGenerator:::.shg_snapshot_root
  base <- tempfile("shg_snap_root_")
  sub  <- file.path(base, "snap-id")
  dir.create(file.path(sub, "params"), recursive = TRUE)
  on.exit(unlink(base, recursive = TRUE), add = TRUE)

  expect_equal(f(base), sub)
})

test_that(".shg_snapshot_root falls back to cache_path when no subdir", {
  f    <- SmokingHistoryGenerator:::.shg_snapshot_root
  base <- tempfile("shg_snap_flat_")
  dir.create(file.path(base, "params"), recursive = TRUE)
  on.exit(unlink(base, recursive = TRUE), add = TRUE)

  expect_equal(f(base), base)
})

# ---------------------------------------------------------------------------
# Local integration test — uses the bundled zip in tests/testdata/.
# Skipped on CRAN (zip excluded from the CRAN tarball via .Rbuildignore)
# and in any environment where the zip is absent.
# ---------------------------------------------------------------------------

test_that("load_params end-to-end: split zips merged and paths configured", {
  skip_on_cran()
  z <- shg_test_split_param_zips()
  shg_test_with_param_cache({
    shg <- new(SHGInterface)
    shg$load_params(smoking_url = z$smok, mortality_url = z$mort)
    expect_equal(shg$initiation_filename, "smoking/initiation.csv")
    expect_equal(shg$smok_params_source, z$smok)
    expect_equal(shg$mort_params_source, z$mort)
    expect_true(file.exists(file.path(shg$input_data_folder, shg$initiation_filename)))
    expect_true(grepl("acm\\.csv$", shg$mortality_filename))
  })
})

test_that("load_params end-to-end: ocm mortality from split zips", {
  skip_on_cran()
  z <- shg_test_split_param_zips()
  shg_test_with_param_cache({
    shg <- new(SHGInterface)
    shg$load_params(smoking_url = z$smok, mortality_url = z$mort, mort_params_type = "ocm")
    expect_equal(shg$mortality_filename, "mortality/ocm-excl-lung-cancer.csv")
  })
})

test_that("load_params: second call reuses cache", {
  skip_on_cran()
  z <- shg_test_split_param_zips()
  shg_test_with_param_cache({
    shg <- new(SHGInterface)
    shg$load_params(smoking_url = z$smok, mortality_url = z$mort)
    combined <- SmokingHistoryGenerator:::.shg_params_combined_cache_path(z$smok, z$mort)
    expect_true(SmokingHistoryGenerator:::.shg_merged_cache_intact(combined, "acm"))
    shg$load_params(smoking_url = z$smok, mortality_url = z$mort)
    expect_true(SmokingHistoryGenerator:::.shg_merged_cache_intact(combined, "acm"))
  })
})

test_that("load_params rejects legacy combined zip as smoking source", {
  skip_on_cran()
  combined <- testthat::test_path("../testdata/usa-national@smok-2018-mort-2016.zip")
  skip_if_not(file.exists(combined))
  z <- shg_test_split_param_zips()
  shg_test_with_param_cache({
    shg <- new(SHGInterface)
    expect_error(
      shg$load_params(smoking_url = combined, mortality_url = z$mort),
      "legacy combined"
    )
  })
})
