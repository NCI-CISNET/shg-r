library(SmokingHistoryGenerator)
library(glue)
library(testthat)

# Helper functions
extract_tag <- function(vector, start_tag, end_tag) {
  i1 <- which(vector %in% start_tag)
  i2 <- which(vector %in% end_tag)
  return(vector[i1[1]:i2[1]])
}

extract_run <- function(vector) {
  return(extract_tag(vector, "<RUN>", "</RUN>"))
}

extract_cessation <- function(vector) {
  cessation <- extract_tag(vector, "<CESSATION_YR>", "</CESSATION_YR>")
  return(cessation[2])
}

get_run_details <- function(file_path) {
  vector <- readLines(file_path)
  run <- extract_run(vector)
  cessation <- extract_cessation(vector)
  return(list(run = run, cessation = cessation))
}

write_input_file_from_template <- function(rng_strategy, yob, cessation_yr, data_folder, outputs_folder) {
  # The main motivation to write custom config files was due to pathing discrepancies between devtools:test() and CMD Check
  template_input <- readLines("../templates/test_input_example.txt")
  input_filepath <- test_path(glue("../inputs/test_input_{rng_strategy}_{yob}_{cessation_yr}.txt"))
  formatted_input <- glue(paste(template_input, collapse = "\n"))
  writeLines(as.character(formatted_input), con = input_filepath)
  return(input_filepath)
}

generate_output <- function(rng_strategy, yob, cessation_yr, outputs_folder) {
  input_filepath = write_input_file_from_template(rng_strategy, yob, cessation_yr, data_folder, outputs_folder)
  shg$LegacyRunWebVersion(input_filepath)
  return(get_run_details(glue("../outputs/test_output_{rng_strategy}_{yob}_{cessation_yr}.txt")))
}

clear_test_artifacts <- function(folder) {
  folder <- test_path(folder)
  if (dir.exists(folder)) {
    files <- list.files(folder, full.names = TRUE)
    file.remove(files)
    file.remove(folder)
  }
}

get_mean_from_column <- function(df, column) {
  return(mean(df[[column]][df[[column]] >= 0]))
}

get_stats_from_df <- function(df) {
  mean_initiation <- get_mean_from_column(df, "smoking_initiation_age")
  mean_cessation <- get_mean_from_column(df, "smoking_cessation_age")
  age_at_death <- get_mean_from_column(df, "age_at_death")
  return(list(mean_initiation = mean_initiation, mean_cessation = mean_cessation, age_at_death = age_at_death))
}
# Tests
shg <- new(SHGInterface)
shg$rng_strategy <- "MersenneTwister"
shg$number_of_segments <- 1
shg$run_multi_threaded <- FALSE
N <- 10^4 # Individuals to simulate (REPEAT)

# TODO: maybe a better way to reference the input data folder in the package?
# when running CMD Check, the ./inst/inputs/default/ folder is found at ../../SmokingHistoryGenerator/inputs/default/
# when running devtools:test(), the ./inst/inputs/default/ folder is found at ../../inst/inputs/default/
# This path is needed for both:
# - the LegacyRunWebVersion(inputfile) (config files must reference the initiation, cessation, etc.)
# - the runSimFromFixedValues call (initiation, cessation, files are expected and found based on input_data_folder

data_folder <- file.path(system.file("/inputs/", package="SmokingHistoryGenerator"), "default")
test_that("SHG inputs/default folder exists", {
  expect_true(file.exists(data_folder))
})

shg$input_data_folder <- data_folder

clear_test_artifacts("../inputs")
clear_test_artifacts("../outputs")
dir.create("../inputs")
dir.create("../outputs")

outputs_folder <- "../outputs"

MT_output_A <- generate_output("MersenneTwister", 1950, 0, outputs_folder)
MT_fixture_A <- get_run_details(test_path("../fixtures/MT/yob_1950_cessation_0.txt"))

MT_output_B <- generate_output("MersenneTwister", 2010, 2050, outputs_folder)
MT_fixture_B <- get_run_details(test_path("../fixtures/MT/yob_2010_cessation_2050.txt"))

test_that("MersenneTwister simulation output in R does not differ from C++ fixtures", {
  expect_equal(MT_output_A$run, MT_fixture_A$run)
  expect_equal(MT_output_A$cessation, "0")
  expect_equal(MT_fixture_A$cessation, "0")
  expect_equal(MT_output_B$run, MT_fixture_B$run)
  expect_equal(MT_output_B$cessation, "2050")
  expect_equal(MT_fixture_B$cessation, "2050")
})

RS_output_A <- generate_output("RngStream", 1950, 0, outputs_folder)
RS_fixture_A <- get_run_details(test_path("../fixtures/RS/yob_1950_cessation_0.txt"))

RS_output_B <- generate_output("RngStream", 2010, 2050, outputs_folder)
RS_fixture_B <- get_run_details(test_path("../fixtures/RS/yob_2010_cessation_2050.txt"))

test_that("RngStream simulation output in R does not differ from C++ fixtures", {
  expect_equal(RS_output_A$run, RS_fixture_A$run)
  expect_equal(RS_output_A$cessation, "0")
  expect_equal(RS_fixture_A$cessation, "0")
  expect_equal(RS_output_B$run, RS_fixture_B$run)
  expect_equal(RS_output_B$cessation, "2050")
  expect_equal(RS_fixture_B$cessation, "2050")
})
shg$rng_strategy <- "MersenneTwister"
MT_SIM <- shg$runSimFromFixedValues(N, 0, 0, 1940)

shg$rng_strategy <- "RngStream"
RS_SIM <- shg$runSimFromFixedValues(N, 0, 0, 1940)

MT_STATS <- get_stats_from_df(MT_SIM)
RS_STATS <- get_stats_from_df(RS_SIM)

test_that("Comparison between MT-SIM and RNGSTREAM-SIM", {
  expect_equal(dim(RS_SIM), dim(MT_SIM))
  expect_equal(MT_STATS$mean_initiation, RS_STATS$mean_initiation, tolerance = 0.01)
  expect_equal(MT_STATS$mean_cessation, RS_STATS$mean_cessation, tolerance = 0.01)
  expect_equal(MT_STATS$mean_age_at_death, RS_STATS$mean_age_at_death, tolerance = 0.01)
  # If MT_STATS and RS_STATS are equal, it would indicate there is a problem with the RNG
  # Results should be very similar but *not* identical
  expect_false(isTRUE(all.equal(MT_STATS, RS_STATS)))
})

pop <- list(
    race = rep(0, N),
    sex = rep(0, N),
    birth_cohort = rep(1940, N)
)

shg$rng_strategy <- "MersenneTwister"
MT_SIM_POP <- shg$runSimFromDataFrame(pop)

shg$rng_strategy <- "RngStream"
RS_SIM_POP <- shg$runSimFromDataFrame(pop)

MT_STATS_POP <- get_stats_from_df(MT_SIM_POP)
RS_STATS_POP <- get_stats_from_df(RS_SIM_POP)

test_that("Comparison between MT-SIM and RNGSTREAM-SIM with runSimFromDataFrame", {
  expect_equal(MT_STATS_POP$mean_initiation, RS_STATS_POP$mean_initiation, tolerance = 0.01)
  expect_equal(MT_STATS_POP$mean_cessation, RS_STATS_POP$mean_cessation, tolerance = 0.01)
  expect_equal(MT_STATS_POP$mean_age_at_death, RS_STATS_POP$mean_age_at_death, tolerance = 0.01)
})

test_that("Comparison between runSimFromDataFrame and runSimFromFixedValues for MersenneTwister", {
  expect_identical(MT_STATS_POP$mean_initiation, MT_STATS$mean_initiation)
  expect_identical(MT_STATS_POP$mean_cessation, MT_STATS$mean_cessation)
  expect_identical(MT_STATS_POP$mean_age_at_death, MT_STATS$mean_age_at_death)
})
test_that("Comparison between runSimFromDataFrame and runSimFromFixedValues for RngStream", {
  expect_identical(RS_STATS_POP$mean_initiation, RS_STATS$mean_initiation)
  expect_identical(RS_STATS_POP$mean_cessation, RS_STATS$mean_cessation)
  expect_identical(RS_STATS_POP$mean_age_at_death, RS_STATS$mean_age_at_death)
})

# TODO: Compare Legacy tests with runSimFromFixedValues(): requires parsing of results
