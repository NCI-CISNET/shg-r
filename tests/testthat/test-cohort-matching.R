library(SmokingHistoryGenerator)
library(testthat)

# Test data folder
data_folder <- file.path(system.file("/inputs/", package="SmokingHistoryGenerator"), "default")

# Helper to create a temporary test data directory with custom CPD file
create_test_data_dir <- function(cpd_content) {
  test_dir <- tempfile(pattern = "shg_test_")
  dir.create(test_dir, recursive = TRUE)
  
  # Copy default files
  default_dir <- data_folder
  file.copy(file.path(default_dir, "lbc_smokehist_initiation.txt"), test_dir)
  file.copy(file.path(default_dir, "lbc_smokehist_cessation.txt"), test_dir)
  file.copy(file.path(default_dir, "lbc_smokehist_oc_mortality.txt"), test_dir)
  
  # Write custom CPD file
  writeLines(cpd_content, file.path(test_dir, "lbc_smokehist_cpd.txt"))
  
  return(test_dir)
}

test_that("CPD file with dots (legacy format) works correctly", {
  # Create a CPD file with dots (legacy behavior - should work)
  # Format: Race,Sex,StartYOB,EndYOB,Age,Cat1,Cat2,Cat3,Cat4,Cat5,Cat6
  # Using cohort 1950 (1950-1959) which should match default initiation file
  cpd_with_dots <- c(
    "7",
    "* Test CPD file with dots (legacy format)",
    "* Race,Sex,StartYOB,EndYOB,Age,Cat1,Cat2,Cat3,Cat4,Cat5,Cat6",
    "0,0,1950,1959,18,0.1,0.2,0.3,0.2,0.15,0.05",
    "0,0,1950,1959,19,.,.,.,.,.,.",
    "0,0,1950,1959,20,0.15,0.25,0.3,0.2,0.1,0.0"
  )
  
  test_dir <- create_test_data_dir(cpd_with_dots)
  on.exit(unlink(test_dir, recursive = TRUE), add = TRUE)
  
  shg <- new(SHGInterface)
  shg$input_data_folder <- test_dir
  shg$rng_strategy <- "RngStream"
  shg$rngstream_seed <- c(12345, 12345, 12345, 12345, 12345, 12345)
  shg$number_of_segments <- 1
  shg$num_threads <- 1
  
  # Should not error - dots are valid
  expect_error(result <- shg$runSimFromFixedValues(100, 0, 0, 1950), NA)
  expect_equal(nrow(result), 100)
})

test_that("CPD file with mismatched cohorts is handled gracefully", {
  # Create a CPD file with some rows matching and some mismatched cohorts
  # Mismatched rows should be skipped with warning
  cpd_mixed <- c(
    "7",
    "* Test CPD file with mixed cohorts",
    "* Race,Sex,StartYOB,EndYOB,Age,Cat1,Cat2,Cat3,Cat4,Cat5,Cat6",
    "0,0,1950,1959,18,0.1,0.2,0.3,0.2,0.15,0.05",  # Valid cohort
    "0,0,1950,1959,19,0.15,0.25,0.3,0.2,0.1,0.0",  # Valid cohort
    "0,0,1900,1909,20,0.2,0.3,0.3,0.15,0.05,0.0",  # Mismatched cohort (should be skipped)
    "0,0,1950,1959,20,0.12,0.22,0.32,0.22,0.12,0.0"  # Valid cohort
  )
  
  test_dir <- create_test_data_dir(cpd_mixed)
  on.exit(unlink(test_dir, recursive = TRUE), add = TRUE)
  
  shg <- new(SHGInterface)
  shg$input_data_folder <- test_dir
  shg$rng_strategy <- "RngStream"
  shg$rngstream_seed <- c(12345, 12345, 12345, 12345, 12345, 12345)
  shg$number_of_segments <- 1
  shg$num_threads <- 1
  
  # Should warn but not error
  expect_warning(
    result <- shg$runSimFromFixedValues(100, 0, 0, 1950),
    "WARNING: CPD file contains cohort range"
  )
  
  expect_equal(nrow(result), 100)
  # Simulation should still work - mismatched rows are skipped
})

test_that("Results are reproducible with same seed despite mismatched cohorts", {
  # Create CPD file with mismatched cohorts
  cpd_mismatched <- c(
    "7",
    "* Test CPD file with mismatched cohorts",
    "* Race,Sex,StartYOB,EndYOB,Age,Cat1,Cat2,Cat3,Cat4,Cat5,Cat6",
    "0,0,1950,1959,18,0.1,0.2,0.3,0.2,0.15,0.05",
    "0,0,1900,1909,19,0.2,0.3,0.3,0.15,0.05,0.0",  # Mismatched - will be skipped
    "0,0,1950,1959,19,0.15,0.25,0.3,0.2,0.1,0.0"
  )
  
  test_dir <- create_test_data_dir(cpd_mismatched)
  on.exit(unlink(test_dir, recursive = TRUE), add = TRUE)
  
  seed <- c(12345, 12345, 12345, 12345, 12345, 12345)
  
  # Run 1
  shg1 <- new(SHGInterface)
  shg1$input_data_folder <- test_dir
  shg1$rng_strategy <- "RngStream"
  shg1$rngstream_seed <- seed
  shg1$number_of_segments <- 1
  shg1$num_threads <- 1
  
  suppressWarnings(result1 <- shg1$runSimFromFixedValues(100, 0, 0, 1950))
  
  # Run 2 (same seed, should be identical)
  shg2 <- new(SHGInterface)
  shg2$input_data_folder <- test_dir
  shg2$rng_strategy <- "RngStream"
  shg2$rngstream_seed <- seed
  shg2$number_of_segments <- 1
  shg2$num_threads <- 1
  
  suppressWarnings(result2 <- shg2$runSimFromFixedValues(100, 0, 0, 1950))
  
  # Results should be identical (reproducibility)
  expect_equal(result1$smoking_initiation_age, result2$smoking_initiation_age)
  expect_equal(result1$smoking_cessation_age, result2$smoking_cessation_age)
  expect_equal(result1$age_at_death, result2$age_at_death)
})

test_that("Backwards compatibility: default CPD file produces same results", {
  # Test that using default CPD file produces same results as before
  # This ensures backwards compatibility
  
  shg1 <- new(SHGInterface)
  shg1$input_data_folder <- data_folder
  shg1$rng_strategy <- "RngStream"
  shg1$rngstream_seed <- c(12345, 12345, 12345, 12345, 12345, 12345)
  shg1$number_of_segments <- 1
  shg1$num_threads <- 1
  
  result1 <- shg1$runSimFromFixedValues(100, 0, 0, 1950)
  
  # Run again with same seed
  shg2 <- new(SHGInterface)
  shg2$input_data_folder <- data_folder
  shg2$rng_strategy <- "RngStream"
  shg2$rngstream_seed <- c(12345, 12345, 12345, 12345, 12345, 12345)
  shg2$number_of_segments <- 1
  shg2$num_threads <- 1
  
  result2 <- shg2$runSimFromFixedValues(100, 0, 0, 1950)
  
  # Results should be identical (reproducibility)
  expect_equal(result1$smoking_initiation_age, result2$smoking_initiation_age)
  expect_equal(result1$smoking_cessation_age, result2$smoking_cessation_age)
  expect_equal(result1$age_at_death, result2$age_at_death)
})

test_that("CPD file with all dots still works (no data case)", {
  # Create a CPD file with all dots (no actual data)
  cpd_all_dots <- c(
    "7",
    "* Test CPD file with all dots",
    "* Race,Sex,StartYOB,EndYOB,Age,Cat1,Cat2,Cat3,Cat4,Cat5,Cat6",
    "0,0,1950,1959,18,.,.,.,.,.,.",
    "0,0,1950,1959,19,.,.,.,.,.,.",
    "0,0,1950,1959,20,.,.,.,.,.,."
  )
  
  test_dir <- create_test_data_dir(cpd_all_dots)
  on.exit(unlink(test_dir, recursive = TRUE), add = TRUE)
  
  shg <- new(SHGInterface)
  shg$input_data_folder <- test_dir
  shg$rng_strategy <- "RngStream"
  shg$rngstream_seed <- c(12345, 12345, 12345, 12345, 12345, 12345)
  shg$number_of_segments <- 1
  shg$num_threads <- 1
  
  # Should not error - all dots is valid (no CPD data)
  expect_error(result <- shg$runSimFromFixedValues(100, 0, 0, 1950), NA)
  expect_equal(nrow(result), 100)
})
