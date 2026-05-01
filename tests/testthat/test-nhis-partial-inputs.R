library(testthat)
library(SmokingHistoryGenerator)

nhis_testdata_root <- function() {
  test_path("../testdata/NHIS-1965-2016")
}

expect_nhis_partial_files <- function(dir, ext) {
  req <- c(
    paste0("initiation.", ext),
    paste0("cessation.", ext),
    "cpd.csv",
    paste0("acm.", ext),
    paste0("ocm-excl-lung-cancer.", ext)
  )
  for (f in req) {
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
    shg$initiation_filename <- "initiation.csv"
    shg$cessation_filename <- "cessation.csv"
    shg$mortality_filename <- "acm.csv"
  }
  shg$cpd_filename <- "cpd.csv"
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
  expect_nhis_partial_files(csv_partial, "csv")
  out <- run_fixed_cohort(csv_partial, legacy_txt = FALSE)
  expect_equal(nrow(out), 120)
  expect_true(all(out$smoking_initiation_age <= 99 | out$smoking_initiation_age == -999))
})

test_that("NHIS legacy-partial fixture exists and runs (wide .txt + preamble; CPD as .csv)", {
  nhis_base <- nhis_testdata_root()
  skip_if_not(
    dir.exists(nhis_base),
    "NHIS test fixtures missing (omitted from `R CMD build` tarball; use a full git checkout)"
  )
  legacy_partial <- normalizePath(file.path(nhis_base, "legacy-partial"), winslash = "/", mustWork = TRUE)
  expect_nhis_partial_files(legacy_partial, "txt")
  expect_true(file.exists(file.path(legacy_partial, "cpd.csv")))
  out <- run_fixed_cohort(legacy_partial, legacy_txt = TRUE)
  expect_equal(nrow(out), 120)
})
