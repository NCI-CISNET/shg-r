library(testthat)

.shg_strip_race_column <- function(src, dest) {
  d <- read.csv(src, check.names = FALSE)
  if ("RACE" %in% names(d)) {
    d$RACE <- NULL
  }
  names(d)[1] <- names(d)[1] # keep SEX first
  write.table(d, dest, sep = ",", row.names = FALSE, col.names = TRUE, quote = FALSE)
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
