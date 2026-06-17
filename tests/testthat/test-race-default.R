library(testthat)

.shg_strip_race_column <- function(src, dest) {
  d <- read.csv(src, check.names = FALSE)
  if ("RACE" %in% names(d)) {
    d$RACE <- NULL
  }
  stopifnot(identical(names(d)[1L], "SEX")) # header starts with SEX when RACE is omitted
  write.table(d, dest, sep = ",", row.names = FALSE, col.names = TRUE, quote = FALSE)
}

.shg_copy_bundled_csvs <- function(src_root, dest_root, strip_race = FALSE) {
  for (subdir in c("smok", "mort")) {
    dest_sub <- file.path(dest_root, subdir)
    dir.create(dest_sub, recursive = TRUE, showWarnings = FALSE)
    src_sub <- file.path(src_root, subdir)
    for (src in list.files(src_sub, pattern = "\\.csv$", full.names = TRUE)) {
      dest <- file.path(dest_sub, basename(src))
      if (strip_race) {
        .shg_strip_race_column(src, dest)
      } else {
        file.copy(src, dest, overwrite = TRUE)
      }
    }
  }
}

.shg_run_race0_fixed <- function(data_dir, n = 80L) {
  shg <- new(SHGInterface)
  shg$input_data_folder <- data_dir
  shg$mortality_filename <- "mort/acm.csv"
  shg$rng_strategy <- "RngStream"
  shg$rngstream_seed <- rep(12345, 6)
  shg$num_threads <- 1L
  shg$runSimFromFixedValues(n, 0L, 0L, 1950L)
}

test_that("LegacyRunWebVersion accepts config without RACE= (defaults to 0)", {
  skip_on_cran()
  shg <- new(SHGInterface)
  data_folder <- normalizePath(
    system.file("extdata", "2018", package = "SmokingHistoryGenerator"),
    winslash = "/",
    mustWork = TRUE
  )
  outputs_folder <- tempfile()
  dir.create(outputs_folder)
  on.exit(unlink(outputs_folder, recursive = TRUE), add = TRUE)
  template_path <- readLines(test_path("../templates/test_input_example.txt"))
  template_path <- template_path[!grepl("^RACE=", template_path)]
  input_filepath <- tempfile(fileext = ".txt")
  on.exit(unlink(input_filepath), add = TRUE)
  txt <- glue::glue(paste(template_path, collapse = "\n"),
    rng_strategy = "RngStream",
    yob = 1950,
    cessation_yr = 0,
    data_folder = data_folder,
    outputs_folder = outputs_folder
  )
  writeLines(as.character(txt), input_filepath)
  expect_no_error(shg$LegacyRunWebVersion(input_filepath))
})

test_that("race-optional CSV tables load and run (implicit race=0)", {
  skip_on_cran()
  src_root <- system.file("extdata", "2018", package = "SmokingHistoryGenerator")
  td <- tempfile()
  dir.create(td)
  dir.create(file.path(td, "smok"))
  dir.create(file.path(td, "mort"))
  on.exit(unlink(td, recursive = TRUE), add = TRUE)

  .shg_strip_race_column(
    file.path(src_root, "smok", "initiation.csv"),
    file.path(td, "smok", "initiation.csv")
  )
  .shg_strip_race_column(
    file.path(src_root, "smok", "cessation.csv"),
    file.path(td, "smok", "cessation.csv")
  )
  .shg_strip_race_column(
    file.path(src_root, "smok", "cpd.csv"),
    file.path(td, "smok", "cpd.csv")
  )
  .shg_strip_race_column(
    file.path(src_root, "mort", "acm.csv"),
    file.path(td, "mort", "acm.csv")
  )

  hdr <- readLines(file.path(td, "smok", "initiation.csv"), n = 1L)
  expect_false(grepl("^RACE,", hdr))

  shg <- new(SHGInterface)
  shg$input_data_folder <- td
  out <- shg$runSimFromFixedValues(50, 0, 0, 1950)
  expect_s3_class(out, "data.frame")
  expect_equal(nrow(out), 50L)
})

test_that("race-optional CSV output matches bundled CSV with race=0", {
  skip_on_cran()
  src_root <- system.file("extdata", "2018", package = "SmokingHistoryGenerator")
  td_full <- tempfile()
  td_norace <- tempfile()
  dir.create(td_full)
  dir.create(td_norace)
  on.exit(unlink(c(td_full, td_norace), recursive = TRUE), add = TRUE)

  .shg_copy_bundled_csvs(src_root, td_full, strip_race = FALSE)
  .shg_copy_bundled_csvs(src_root, td_norace, strip_race = TRUE)

  hdr <- readLines(file.path(td_norace, "smok", "initiation.csv"), n = 1L)
  expect_false(grepl("^RACE,", hdr))
  hdr_full <- readLines(file.path(td_full, "smok", "initiation.csv"), n = 1L)
  expect_true(grepl("^RACE,", hdr_full))

  out_full <- .shg_run_race0_fixed(td_full)
  out_norace <- .shg_run_race0_fixed(td_norace)
  expect_equal(out_norace, out_full)
})
