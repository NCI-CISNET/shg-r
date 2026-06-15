# Regression: RngStream multi-segment runs where repeat %% segments != 0 must match the
# CLI segment split (first `remainder` segments get one extra individual). The R wrapper
# previously put the entire remainder on the last segment only.

library(SmokingHistoryGenerator)
library(testthat)

SHGInterface <- getFromNamespace("SHGInterface", "SmokingHistoryGenerator")

rs_fp <- function(d) {
  ia <- d$smoking_initiation_age
  ca <- d$smoking_cessation_age
  c(
    sum(d$age_at_death),
    sum(ia[ia >= 0]),
    sum(ca[ca >= 0])
  )
}

test_that("RngStream: non-divisible repeat with 8 segments (206 %% 8 != 0) matches regression fingerprint", {
  shg <- new(SHGInterface)
  shg$input_data_folder <- system.file("extdata", "2018", package = "SmokingHistoryGenerator")
  shg$mortality_filename <- "mort/acm.csv"
  shg$rng_strategy <- "RngStream"
  shg$rngstream_seed <- rep(12345, 6)
  shg$num_threads <- 1L
  shg$number_of_segments <- 8L
  d <- shg$runSimFromFixedValues(206L, 0L, 0L, 1950L)
  expect_equal(nrow(d), 206L)
  expect_equal(
    rs_fp(d),
    c(4835L, 2024L, 6333L),
    label = "sum(age_at_death), sum(init>=0), sum(cess>=0) — update if engine tables change intentionally"
  )
})

test_that("RngStream: non-divisible repeat with 7 segments (503 %% 7 != 0) matches regression fingerprint", {
  shg <- new(SHGInterface)
  shg$input_data_folder <- system.file("extdata", "2018", package = "SmokingHistoryGenerator")
  shg$mortality_filename <- "mort/acm.csv"
  shg$rng_strategy <- "RngStream"
  shg$rngstream_seed <- rep(12345, 6)
  shg$num_threads <- 1L
  shg$number_of_segments <- 7L
  d <- shg$runSimFromFixedValues(503L, 0L, 0L, 1950L)
  expect_equal(nrow(d), 503L)
  expect_equal(
    rs_fp(d),
    c(13564L, 5452L, 16266L),
    label = "sum(age_at_death), sum(init>=0), sum(cess>=0) — update if engine tables change intentionally"
  )
})

test_that("RngStream: memory vs file output agree when repeat %% segments != 0", {
  output_path <- tempfile(fileext = ".csv")
  on.exit(unlink(output_path), add = TRUE)

  df <- data.frame(
    race = as.integer(rep(0, 206)),
    sex = as.integer(rep(0, 206)),
    birth_cohort = as.integer(rep(1950, 206))
  )

  common <- function(shg) {
    shg$input_data_folder <- system.file("extdata", "2018", package = "SmokingHistoryGenerator")
    shg$mortality_filename <- "mort/acm.csv"
    shg$rng_strategy <- "RngStream"
    shg$rngstream_seed <- rep(12345, 6)
    shg$cpd_format <- "none"
    shg$num_threads <- 1L
    shg$number_of_segments <- 8L
    shg
  }

  shg_mem <- common(new(SHGInterface))
  mem <- shg_mem$runSimFromDataFrame(df)

  shg_file <- common(new(SHGInterface))
  shg_file$output_file <- output_path
  shg_file$runSimFromDataFrame(df)

  lines <- readLines(output_path, warn = FALSE, encoding = "UTF-8")
  lines <- gsub("\r", "", lines, fixed = TRUE)
  trimmed <- trimws(lines)
  i0 <- which(trimmed == "<RUN>")
  i1 <- which(trimmed == "</RUN>")
  expect_true(length(i0) && length(i1))
  body <- lines[(i0[1L] + 1L):(i1[1L] - 1L)]
  body <- body[nzchar(trimws(body))]
  is_row <- grepl("^[0-9-]", trimws(body)) & grepl(";", body, fixed = TRUE)
  body <- body[is_row]
  expect_equal(length(body), 206L)

  parse_core <- function(ln) {
    p <- strsplit(ln, ";", fixed = TRUE)[[1L]]
    as.integer(p[4:6])
  }
  # vapply -> 3 x nrow matrix: row1=initiation, row2=cessation, row3=age_at_death
  mat <- vapply(body, parse_core, integer(3))

  expect_equal(mem$smoking_initiation_age, as.integer(mat[1L, ]))
  expect_equal(mem$smoking_cessation_age, as.integer(mat[2L, ]))
  expect_equal(mem$age_at_death, as.integer(mat[3L, ]))
})
