library(SmokingHistoryGenerator)
library(glue)
library(testthat)

extract_tag <- function(vector, tag) {
  # Find all occurrences of start and end tags
  start_tag <- paste0("<", tag, ">")
  end_tag <- paste0("</", tag, ">")

  start_indices <- which(grepl(start_tag, vector, fixed = TRUE))
  end_indices <- which(grepl(end_tag, vector, fixed = TRUE))
 
  # Check if tags exist
  if (length(start_indices) == 0 || length(end_indices) == 0) {
    return(NULL)
  }
  
  # Handle single-line case
  if (any(start_indices == end_indices)) {
    single_line_idx <- intersect(start_indices, end_indices)[1]
    line_content <- vector[single_line_idx]
    content <- gsub(paste0(".*", start_tag, "(.+)", end_tag, ".*"), "\\1", line_content)
    return(content)
  }
  
  # Handle multi-line case
  first_start <- start_indices[1]+1
  first_end <- end_indices[end_indices > first_start][1]-1
  
  if (is.na(first_end)) {
    return(NULL)
  }

  return(vector[first_start:first_end])
}

get_run_details <- function(file_path) {
  vector <- readLines(file_path)
  run <- extract_tag(vector, "RUN")
  cessation <- extract_tag(vector, "CESSATION_YR")
  return(list(run = run, cessation = cessation))
}

write_input_file_from_template <- function(template_path, rng_strategy, yob, cessation_yr, data_folder, outputs_folder) {
  # The main motivation to write custom config files was due to pathing discrepancies between devtools:test() and CMD Check
  input_filepath <- test_path(glue("../inputs/test_input_{rng_strategy}_{yob}_{cessation_yr}.txt"))
  formatted_input <- glue(paste(template_path, collapse = "\n"))
  writeLines(as.character(formatted_input), con = input_filepath)
  return(input_filepath)
}

generate_output <- function(rng_strategy, yob, cessation_yr, outputs_folder) {
  template_path <- readLines("../templates/test_input_example.txt")
  input_filepath <- write_input_file_from_template(template_path, rng_strategy, yob, cessation_yr, data_folder, outputs_folder)
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

test_that("Invalid input configuration path fails with proper error message", {
  input_filepath <- "file_does_not_exist.txt"
  expect_error(shg$LegacyRunWebVersion(input_filepath), "The specified input file 'file_does_not_exist.txt' could not be opened for reading.")
})

test_that("Invalid input parameter path (eg initiation) fails with proper error message", {
  # Test when initiation input file doesn't exist
  template_path <- readLines("../templates/test_input_example_incorrect_init_path.txt")
  input_filepath <- write_input_file_from_template(template_path, "MersenneTwister", 1950, 0, data_folder, outputs_folder)
  expect_warning(shg$LegacyRunWebVersion(input_filepath), "^SimException: Error: The specified input file")
})

test_that("Invalid output path fails with proper error message", {
  template_path <- readLines("../templates/test_input_example.txt")
  outputs_folder <- "folder_does_not_exist"
  input_filepath <- write_input_file_from_template(template_path, "MersenneTwister", 1950, 0, data_folder, outputs_folder)
  expect_error(shg$LegacyRunWebVersion(input_filepath), "Specified error file: 'folder_does_not_exist/test_errors_MersenneTwister_1950_0.txt' could not be opened for writing.")
})

# TODO: Compare Legacy tests with runSimFromFixedValues(): requires parsing of results

# Tests for configuration management
test_that("getConfig() returns correct structure with config_version", {
  shg_test <- new(SHGInterface)
  config <- shg_test$getConfig(debug = FALSE)
  
  expect_true(is.list(config))
  expect_equal(config$config_version, "1.0")
  expect_true("rng_strategy" %in% names(config))
  expect_true("number_of_segments" %in% names(config))
  expect_true("run_multi_threaded" %in% names(config))
  expect_true("seeds" %in% names(config))
  expect_true("input_data_folder" %in% names(config))
  expect_true("timestamp" %in% names(config))
})

test_that("getConfig(debug=TRUE) includes debug info", {
  shg_test <- new(SHGInterface)
  config <- shg_test$getConfig(debug = TRUE)
  
  expect_true("rng_state_fingerprint" %in% names(config))
  expect_true("package_version" %in% names(config))
  expect_true("package_source" %in% names(config))
  expect_true("r_version" %in% names(config))
  expect_true("platform" %in% names(config))
  expect_true("memory_usage" %in% names(config))
  expect_true(nchar(config$package_version) > 0)
  expect_true(nchar(config$r_version) > 0)
})

test_that("useConfig() correctly configures instance", {
  shg1 <- new(SHGInterface)
  shg1$rng_strategy <- "RngStream"
  shg1$number_of_segments <- 4
  shg1$run_multi_threaded <- TRUE
  shg1$input_data_folder <- "/test/path"
  shg1$immediate_cessation_year <- 2025
  
  config <- shg1$getConfig(debug = FALSE)
  
  shg2 <- new(SHGInterface)
  shg2$useConfig(config)
  
  expect_equal(shg2$rng_strategy, shg1$rng_strategy)
  expect_equal(shg2$number_of_segments, shg1$number_of_segments)
  expect_equal(shg2$run_multi_threaded, shg1$run_multi_threaded)
  expect_equal(shg2$input_data_folder, shg1$input_data_folder)
  expect_equal(shg2$immediate_cessation_year, shg1$immediate_cessation_year)
})

test_that("useConfig() validates config_version", {
  shg_test <- new(SHGInterface)
  
  # Missing config_version should warn but work
  config_no_version <- list(rng_strategy = "RngStream")
  expect_warning(shg_test$useConfig(config_no_version), "Config missing config_version")
  
  # Unsupported version should warn
  config_bad_version <- list(config_version = "2.0", rng_strategy = "RngStream")
  expect_warning(shg_test$useConfig(config_bad_version), "may not be fully supported")
})

test_that("useConfig() warns on unknown fields", {
  shg_test <- new(SHGInterface)
  config <- list(
    config_version = "1.0",
    rng_strategy = "RngStream",
    unknown_field = "test"
  )
  expect_warning(shg_test$useConfig(config), "Unknown config field")
})

test_that("Round-trip: getConfig() -> useConfig() -> verify", {
  shg1 <- new(SHGInterface)
  shg1$rng_strategy <- "MersenneTwister"
  shg1$number_of_segments <- 1
  shg1$run_multi_threaded <- FALSE
  shg1$immediate_cessation_year <- 2020
  
  config <- shg1$getConfig(debug = FALSE)
  
  # Save and reload simulation
  temp_file <- tempfile(fileext = ".rds")
  saveRDS(config, temp_file)
  config_loaded <- readRDS(temp_file)
  
  shg2 <- new(SHGInterface)
  shg2$useConfig(config_loaded)
  
  expect_equal(shg2$rng_strategy, shg1$rng_strategy)
  expect_equal(shg2$number_of_segments, shg1$number_of_segments)
  expect_equal(shg2$run_multi_threaded, shg1$run_multi_threaded)
  expect_equal(shg2$immediate_cessation_year, shg1$immediate_cessation_year)
  
  unlink(temp_file)
})

test_that("Constructor with config parameter works", {
  config <- list(
    config_version = "1.0",
    rng_strategy = "RngStream",
    number_of_segments = 4,
    run_multi_threaded = TRUE,
    immediate_cessation_year = 2025
  )
  
  shg <- new(SHGInterface, config = config)
  
  expect_equal(shg$rng_strategy, "RngStream")
  expect_equal(shg$number_of_segments, 4)
  expect_equal(shg$run_multi_threaded, TRUE)
  expect_equal(shg$immediate_cessation_year, 2025)
})

test_that("Constructor with empty config works", {
  shg <- new(SHGInterface, config = list())
  # Should use defaults
  expect_equal(shg$rng_strategy, "RngStream")
  expect_equal(shg$number_of_segments, 1)
})

# Tests for MersenneTwister restrictions
test_that("MersenneTwister cannot be used with multiple segments", {
  shg_test <- new(SHGInterface)
  shg_test$rng_strategy <- "MersenneTwister"
  expect_error(shg_test$number_of_segments <- 2, "MersenneTwister RNG cannot maintain IID properties with multiple segments")
})

test_that("MersenneTwister cannot be used with parallel execution", {
  shg_test <- new(SHGInterface)
  shg_test$rng_strategy <- "MersenneTwister"
  expect_error(shg_test$run_multi_threaded <- TRUE, "MersenneTwister RNG cannot maintain IID properties with parallel execution")
})

test_that("Switching to MersenneTwister resets segments and parallel to valid values", {
  shg_test <- new(SHGInterface)
  shg_test$rng_strategy <- "RngStream"
  shg_test$number_of_segments <- 4
  shg_test$run_multi_threaded <- TRUE
  
  expect_warning(
    expect_warning(
      shg_test$rng_strategy <- "MersenneTwister",
      "Resetting number_of_segments to 1"
    ),
    "Resetting run_multi_threaded to FALSE"
  )
  expect_equal(shg_test$number_of_segments, 1)
  expect_equal(shg_test$run_multi_threaded, FALSE)
})

test_that("Parallel execution requires multiple segments", {
  shg_test <- new(SHGInterface)
  shg_test$number_of_segments <- 1
  expect_error(shg_test$run_multi_threaded <- TRUE, "run_multi_threaded cannot be TRUE when number_of_segments is 1")
})

test_that("MersenneTwister with multiple segments is reset to 1 segment", {
  shg_test <- new(SHGInterface)
  shg_test$input_data_folder <- data_folder
  # Use RngStream then switch to MersenneTwister
  shg_test$rng_strategy <- "RngStream"
  shg_test$number_of_segments <- 2
  expect_warning(shg_test$rng_strategy <- "MersenneTwister", "Resetting number_of_segments to 1")
  expect_equal(shg_test$number_of_segments, 1)
  
  # Should work fine with 1 segment
  pop <- list(
    race = rep(0, 100),
    sex = rep(0, 100),
    birth_cohort = rep(1940, 100)
  )
  result <- shg_test$runSimFromDataFrame(pop)
  expect_equal(nrow(result), 100)
})

test_that("RngStream allows multiple segments and parallel execution", {
  shg_test <- new(SHGInterface)
  shg_test$rng_strategy <- "RngStream"
  shg_test$number_of_segments <- 4
  shg_test$run_multi_threaded <- TRUE
  expect_equal(shg_test$rng_strategy, "RngStream")
  expect_equal(shg_test$number_of_segments, 4)
  expect_equal(shg_test$run_multi_threaded, TRUE)
})

test_that("MersenneTwister: custom seeds produce different results and reverting to defaults restores original results", {
  N <- 1000
  shg_mt <- new(SHGInterface)
  shg_mt$input_data_folder <- data_folder
  shg_mt$rng_strategy <- "MersenneTwister"
  shg_mt$number_of_segments <- 1
  shg_mt$run_multi_threaded <- FALSE
  
  # Baseline: run with default seeds (no seeds set)
  baseline <- shg_mt$runSimFromFixedValues(N, 0, 0, 1940)
  baseline_stats <- get_stats_from_df(baseline)
  
  # Run with custom seeds
  shg_mt$mt_seeds <- c(1111111111, 2222222222, 3333333333, 4444444444)
  custom_seed_run <- shg_mt$runSimFromFixedValues(N, 0, 0, 1940)
  custom_stats <- get_stats_from_df(custom_seed_run)
  
  # Verify custom seeds produce different results
  expect_false(isTRUE(all.equal(baseline_stats, custom_stats)))
  
  # Run with different custom seeds
  shg_mt$mt_seeds <- c(9999999999, 8888888888, 7777777777, 6666666666)
  different_seed_run <- shg_mt$runSimFromFixedValues(N, 0, 0, 1940)
  different_stats <- get_stats_from_df(different_seed_run)
  
  # Verify different seeds produce different results
  expect_false(isTRUE(all.equal(custom_stats, different_stats)))
  
  # Revert to defaults by manually setting default seeds
  # Default MT seeds: 1898587603, 1468371936, 1551308340, 1590227640
  shg_mt$mt_seeds <- c(1898587603, 1468371936, 1551308340, 1590227640)
  
  reset_run <- shg_mt$runSimFromFixedValues(N, 0, 0, 1940)
  reset_stats <- get_stats_from_df(reset_run)
  
  # Verify reverting to defaults produces same results as baseline
  expect_equal(baseline_stats$mean_initiation, reset_stats$mean_initiation)
  expect_equal(baseline_stats$mean_cessation, reset_stats$mean_cessation)
  expect_equal(baseline_stats$age_at_death, reset_stats$age_at_death)
})

test_that("RngStream: custom seed produces different results and reverting to defaults restores original results", {
  N <- 1000
  shg_rs <- new(SHGInterface)
  shg_rs$input_data_folder <- data_folder
  shg_rs$rng_strategy <- "RngStream"
  shg_rs$number_of_segments <- 1
  shg_rs$run_multi_threaded <- FALSE
  
  # Baseline: run with default seed (no seed set)
  baseline <- shg_rs$runSimFromFixedValues(N, 0, 0, 1940)
  baseline_stats <- get_stats_from_df(baseline)
  
  # Run with custom seed
  shg_rs$rngstream_seed <- c(11111, 22222, 33333, 44444, 55555, 66666)
  custom_seed_run <- shg_rs$runSimFromFixedValues(N, 0, 0, 1940)
  custom_stats <- get_stats_from_df(custom_seed_run)
  
  # Verify custom seed produces different results
  expect_false(isTRUE(all.equal(baseline_stats, custom_stats)))
  
  # Run with different custom seed
  shg_rs$rngstream_seed <- c(99999, 88888, 77777, 66666, 55555, 44444)
  different_seed_run <- shg_rs$runSimFromFixedValues(N, 0, 0, 1940)
  different_stats <- get_stats_from_df(different_seed_run)
  
  # Verify different seed produces different results
  expect_false(isTRUE(all.equal(custom_stats, different_stats)))
  
  # Revert to defaults by manually setting default seed
  # Default RngStream seed: c(12345, 12345, 12345, 12345, 12345, 12345)
  shg_rs$rngstream_seed <- c(12345, 12345, 12345, 12345, 12345, 12345)
  
  reset_run <- shg_rs$runSimFromFixedValues(N, 0, 0, 1940)
  reset_stats <- get_stats_from_df(reset_run)
  
  # Verify reverting to defaults produces same results as baseline
  expect_equal(baseline_stats$mean_initiation, reset_stats$mean_initiation)
  expect_equal(baseline_stats$mean_cessation, reset_stats$mean_cessation)
  expect_equal(baseline_stats$age_at_death, reset_stats$age_at_death)
})

test_that("get_current_seeds() returns correct seeds based on RNG strategy", {
  shg_mt <- new(SHGInterface)
  shg_mt$rng_strategy <- "MersenneTwister"
  shg_mt$mt_seeds <- c(1111111111, 2222222222, 3333333333, 4444444444)
  
  # Should return MT seeds when MT strategy is selected
  current_seeds <- shg_mt$get_current_seeds()
  expect_equal(length(current_seeds), 4)
  expect_equal(current_seeds, c(1111111111, 2222222222, 3333333333, 4444444444))
  
  shg_rs <- new(SHGInterface)
  shg_rs$rng_strategy <- "RngStream"
  shg_rs$rngstream_seed <- c(11111, 22222, 33333, 44444, 55555, 66666)
  
  # Should return RngStream seed when RngStream strategy is selected
  current_seeds_rs <- shg_rs$get_current_seeds()
  expect_equal(length(current_seeds_rs), 6)
  expect_equal(current_seeds_rs, c(11111, 22222, 33333, 44444, 55555, 66666))
})

test_that("reset_seeds_to_defaults() resets seeds to default values", {
  N <- 1000
  shg_mt <- new(SHGInterface)
  shg_mt$input_data_folder <- data_folder
  shg_mt$rng_strategy <- "MersenneTwister"
  shg_mt$number_of_segments <- 1
  shg_mt$run_multi_threaded <- FALSE
  
  # Set custom seeds
  shg_mt$mt_seeds <- c(1111111111, 2222222222, 3333333333, 4444444444)
  custom_run <- shg_mt$runSimFromFixedValues(N, 0, 0, 1940)
  custom_stats <- get_stats_from_df(custom_run)
  
  # Reset to defaults using the method
  shg_mt$reset_seeds_to_defaults()
  
  # Verify seeds were reset
  current_seeds <- shg_mt$get_current_seeds()
  expect_equal(current_seeds, c(1898587603, 1468371936, 1551308340, 1590227640))
  
  # Verify reset produces same results as baseline
  baseline <- shg_mt$runSimFromFixedValues(N, 0, 0, 1940)
  baseline_stats <- get_stats_from_df(baseline)
  
  # Create a fresh instance for comparison
  shg_baseline <- new(SHGInterface)
  shg_baseline$input_data_folder <- data_folder
  shg_baseline$rng_strategy <- "MersenneTwister"
  shg_baseline$number_of_segments <- 1
  shg_baseline$run_multi_threaded <- FALSE
  # Don't set seeds, so defaults will be used
  baseline_fresh <- shg_baseline$runSimFromFixedValues(N, 0, 0, 1940)
  baseline_fresh_stats <- get_stats_from_df(baseline_fresh)
  
  expect_equal(baseline_stats$mean_initiation, baseline_fresh_stats$mean_initiation)
  expect_equal(baseline_stats$mean_cessation, baseline_fresh_stats$mean_cessation)
  expect_equal(baseline_stats$age_at_death, baseline_fresh_stats$age_at_death)
  
  # Test RngStream reset
  shg_rs <- new(SHGInterface)
  shg_rs$input_data_folder <- data_folder
  shg_rs$rng_strategy <- "RngStream"
  shg_rs$number_of_segments <- 1
  shg_rs$run_multi_threaded <- FALSE
  
  # Set custom seed
  shg_rs$rngstream_seed <- c(11111, 22222, 33333, 44444, 55555, 66666)
  
  # Reset to defaults using the method
  shg_rs$reset_seeds_to_defaults()
  
  # Verify seeds were reset
  current_seeds_rs <- shg_rs$get_current_seeds()
  expect_equal(current_seeds_rs, c(12345, 12345, 12345, 12345, 12345, 12345))
})

test_that("get_rng_state_fingerprint() verifies seeds actually affect RNG internal state", {
  shg_mt1 <- new(SHGInterface)
  shg_mt1$input_data_folder <- data_folder
  shg_mt1$rng_strategy <- "MersenneTwister"
  shg_mt1$mt_seeds <- c(1111111111, 2222222222, 3333333333, 4444444444)
  
  # Get fingerprint with custom seeds
  fingerprint1 <- shg_mt1$get_rng_state_fingerprint()
  expect_equal(length(fingerprint1), 12)  # MT returns 12 values (3 from each of 4 streams)
  
  # Set different seeds
  shg_mt1$mt_seeds <- c(9999999999, 8888888888, 7777777777, 6666666666)
  fingerprint2 <- shg_mt1$get_rng_state_fingerprint()
  
  # Verify different seeds produce different fingerprints
  expect_false(isTRUE(all.equal(fingerprint1, fingerprint2)))
  
  # Reset to defaults
  shg_mt1$reset_seeds_to_defaults()
  fingerprint_default <- shg_mt1$get_rng_state_fingerprint()
  
  # Verify default seeds produce different fingerprint than custom seeds
  expect_false(isTRUE(all.equal(fingerprint1, fingerprint_default)))
  expect_false(isTRUE(all.equal(fingerprint2, fingerprint_default)))
  
  # Test RngStream
  shg_rs1 <- new(SHGInterface)
  shg_rs1$input_data_folder <- data_folder
  shg_rs1$rng_strategy <- "RngStream"
  shg_rs1$rngstream_seed <- c(11111, 22222, 33333, 44444, 55555, 66666)
  
  # Get fingerprint with custom seed
  fingerprint_rs1 <- shg_rs1$get_rng_state_fingerprint()
  expect_equal(length(fingerprint_rs1), 24)  # RngStream returns 24 values (6 from each of 4 streams)
  
  # Set different seed
  shg_rs1$rngstream_seed <- c(99999, 88888, 77777, 66666, 55555, 44444)
  fingerprint_rs2 <- shg_rs1$get_rng_state_fingerprint()
  
  # Verify different seeds produce different fingerprints
  expect_false(isTRUE(all.equal(fingerprint_rs1, fingerprint_rs2)))
  
  # Reset to defaults
  shg_rs1$reset_seeds_to_defaults()
  fingerprint_rs_default <- shg_rs1$get_rng_state_fingerprint()
  
  # Verify default seed produces different fingerprint than custom seeds
  expect_false(isTRUE(all.equal(fingerprint_rs1, fingerprint_rs_default)))
  expect_false(isTRUE(all.equal(fingerprint_rs2, fingerprint_rs_default)))
  
  # Verify that same seeds produce same fingerprints (for RngStream, which returns actual state)
  shg_rs2 <- new(SHGInterface)
  shg_rs2$input_data_folder <- data_folder
  shg_rs2$rng_strategy <- "RngStream"
  shg_rs2$rngstream_seed <- c(11111, 22222, 33333, 44444, 55555, 66666)
  fingerprint_rs2_same <- shg_rs2$get_rng_state_fingerprint()
  
  # RngStream should produce identical fingerprints for same seed
  expect_equal(fingerprint_rs1, fingerprint_rs2_same)
})

# TODO: Compare Legacy tests with runSimFromFixedValues(): requires parsing of results
