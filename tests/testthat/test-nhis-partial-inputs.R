library(testthat)
library(SmokingHistoryGenerator)

nhis_testdata_root <- function() {
  test_path("../testdata/2018")
}

expect_nhis_csv_partial_tree <- function(dir) {
  for (f in c(
    file.path("smok", "initiation.csv"),
    file.path("smok", "cessation.csv"),
    file.path("smok", "cpd.csv"),
    file.path("mort", "acm.csv"),
    file.path("mort", "ocm-excl-lung-cancer.csv")
  )) {
    expect_true(file.exists(file.path(dir, f)), info = f)
  }
}

expect_nhis_legacy_partial <- function(dir) {
  for (f in c(
    "initiation.txt",
    "cessation.txt",
    "cpd.txt",
    "acm.txt",
    "ocm-excl-lung-cancer.txt"
  )) {
    expect_true(file.exists(file.path(dir, f)), info = f)
  }
}

run_fixed_cohort <- function(dir, legacy_txt) {
  shg <- new(SHGInterface)
  shg$input_data_folder <- dir
  if (legacy_txt) {
    shg$initiation_filename <- "initiation.txt"
    shg$cessation_filename <- "cessation.txt"
    shg$mortality_filename <- "acm.txt"
  } else {
    shg$initiation_filename <- "smok/initiation.csv"
    shg$cessation_filename <- "smok/cessation.csv"
    shg$mortality_filename <- "mort/acm.csv"
  }
  shg$cpd_filename <- if (legacy_txt) "cpd.txt" else "smok/cpd.csv"
  shg$rng_strategy <- "RngStream"
  shg$num_threads <- 1
  shg$number_of_segments <- 1
  shg$runSimFromFixedValues(120, 0, 0, 1950)
}

test_that("NHIS csv-partial fixture exists and runs (CSV tables, no dot-only CPD rows)", {
  nhis_base <- nhis_testdata_root()
  skip_if_not(
    dir.exists(nhis_base),
    "NHIS test fixtures missing (omitted from `R CMD build` tarball; use a full git checkout)"
  )
  csv_partial <- normalizePath(file.path(nhis_base, "csv-partial"), winslash = "/", mustWork = TRUE)
  expect_nhis_csv_partial_tree(csv_partial)
  out <- run_fixed_cohort(csv_partial, legacy_txt = FALSE)
  expect_equal(nrow(out), 120)
  expect_true(all(out$smoking_initiation_age <= 99 | out$smoking_initiation_age == -999))
})

test_that("NHIS legacy-partial fixture exists and runs (wide .txt + preamble)", {
  nhis_base <- nhis_testdata_root()
  skip_if_not(
    dir.exists(nhis_base),
    "NHIS test fixtures missing (omitted from `R CMD build` tarball; use a full git checkout)"
  )
  legacy_partial <- normalizePath(file.path(nhis_base, "legacy-partial"), winslash = "/", mustWork = TRUE)
  expect_nhis_legacy_partial(legacy_partial)
  out <- run_fixed_cohort(legacy_partial, legacy_txt = TRUE)
  expect_equal(nrow(out), 120)
})
