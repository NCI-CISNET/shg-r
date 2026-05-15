library(SmokingHistoryGenerator)
library(glue)
library(testthat)

# Normalize CRLF / stray \\r for cross-platform comparison (Windows may emit CRLF in file output).
read_output_lines <- function(path) {
  lines <- readLines(path, warn = FALSE, encoding = "UTF-8")
  if (length(lines)) {
    lines[1] <- sub("^\ufeff", "", lines[1])
  }
  gsub("\r", "", lines, fixed = TRUE)
}

# Locate the legacy XML <RUN>...</RUN> data block (tolerant of whitespace).
xml_run_bounds <- function(lines) {
  trimmed <- trimws(lines)
  run_start <- which(trimmed == "<RUN>")
  run_end <- which(trimmed == "</RUN>")
  list(start = run_start, end = run_end)
}

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

# Legacy XML goldens: compare only tags that define the simulation rows, not <RUNINFO>.
# <RUNINFO>/<DATAFILES> paths differ by machine, cwd, and devtools vs R CMD check; goldens
# use portable inst/extdata + tests/... strings. extract_tag(..., "RUN") ignores all of that.
get_run_details <- function(file_path) {
  vector <- read_output_lines(file_path)
  run <- extract_tag(vector, "RUN")
  cessation <- extract_tag(vector, "CESSATION_YR")
  return(list(run = run, cessation = cessation))
}

# Core version embedded in legacy XML goldens (CLI WriteRunInfoTag). When goldens are pinned to
# an older CLI (e.g. 6.4.0 + wide .txt inputs), R parity checks against the current engine skip.
read_fixture_core_ver <- function(file_path) {
  v <- extract_tag(read_output_lines(file_path), "VERSION")
  if (is.null(v) || !length(v)) {
    return(NA_character_)
  }
  as.character(v[[1]])
}

write_input_file_from_template <- function(template_path, rng_strategy, yob, cessation_yr, data_folder, outputs_folder) {
  # The main motivation to write custom config files was due to pathing discrepancies between devtools:test() and CMD Check
  input_filepath <- test_path(glue("../inputs/test_input_{rng_strategy}_{yob}_{cessation_yr}.txt"))
  formatted_input <- glue(paste(template_path, collapse = "\n"))
  writeLines(as.character(formatted_input), con = input_filepath)
  return(input_filepath)
}

generate_output <- function(rng_strategy, yob, cessation_yr, outputs_dir_abs) {
  template_path <- readLines(test_path("../templates/test_input_example.txt"))
  input_filepath <- write_input_file_from_template(
    template_path, rng_strategy, yob, cessation_yr, data_folder, outputs_dir_abs
  )
  shg$LegacyRunWebVersion(input_filepath)
  out_file <- file.path(outputs_dir_abs, glue("test_output_{rng_strategy}_{yob}_{cessation_yr}.txt"))
  return(get_run_details(out_file))
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

# Integer race/sex/birth_cohort columns for runSimFromDataFrame (typed literals hidden here)
test_pop_df <- function(n, race = 0, sex = 0, birth_cohort = 1940) {
  data.frame(
    race = as.integer(rep(race, n)),
    sex = as.integer(rep(sex, n)),
    birth_cohort = as.integer(rep(birth_cohort, n))
  )
}

# Tests
shg <- new(SHGInterface)
# Legacy XML fixtures were generated with ACM (all-cause) mortality tables
shg$mortality_filename <- "mortality/acm.csv"
shg$num_threads <- 1
shg$number_of_segments <- 1
shg$rng_strategy <- "MersenneTwister"
N <- 10^4 # Individuals to simulate (REPEAT)

# TODO: maybe a better way to reference the input data folder in the package?
# Bundled CSV inputs install under system.file("extdata", "2018", package=...) (see inst/extdata in source).
# LegacyRunWebVersion ignores input_data_folder; config paths are cwd-relative or absolute.

data_folder <- system.file("extdata", "2018", package = "SmokingHistoryGenerator")
test_that("SHG extdata folder exists and contains bundled CSV inputs", {
  expect_true(nzchar(data_folder) && dir.exists(data_folder))
  expect_true(file.exists(file.path(data_folder, "smoking", "initiation.csv")))
})

shg$input_data_folder <- data_folder

clear_test_artifacts("../inputs")
clear_test_artifacts("../outputs")
dir.create(test_path("../inputs"), recursive = TRUE, showWarnings = FALSE)
dir.create(test_path("../outputs"), recursive = TRUE, showWarnings = FALSE)
outputs_dir_abs <- normalizePath(test_path("../outputs"), winslash = "/", mustWork = FALSE)

outputs_folder <- outputs_dir_abs
MT_output_A <- generate_output("MersenneTwister", 1950, 0, outputs_folder)
MT_fixture_A <- get_run_details(test_path("../fixtures/2018/MT/yob_1950_cessation_0.txt"))

MT_output_B <- generate_output("MersenneTwister", 2010, 2050, outputs_folder)
MT_fixture_B <- get_run_details(test_path("../fixtures/2018/MT/yob_2010_cessation_2050.txt"))

# One canonical fixture per scenario: same config must produce the same <RUN> lines on every OS.
# If a platform diverges, fix determinism in the engine — do not maintain alternate goldens or relaxed checks.
# Path-agnostic: we pass only the <RUN>...</RUN> line vectors from get_run_details(), never whole-file XML.
compare_legacy_run_body <- function(actual_run, fixture_run) {
  expect_equal(actual_run, fixture_run)
}

fixture_core <- read_fixture_core_ver(test_path("../fixtures/2018/MT/yob_1950_cessation_0.txt"))
pkg_core <- shg$get_shg_core_version()
fixture_skip_msg <- paste0(
  "2018 XML goldens are core ", fixture_core, "; this build is ", pkg_core,
  ". Regenerate with tools/regenerate-legacy-xml-fixtures-cli.sh using a matching CLI, or upgrade goldens."
)

test_that("MersenneTwister simulation output in R does not differ from C++ fixtures", {
  skip_if(!is.na(fixture_core) && !identical(fixture_core, pkg_core), fixture_skip_msg)
  compare_legacy_run_body(MT_output_A$run, MT_fixture_A$run)
  expect_equal(MT_output_A$cessation, "0")
  expect_equal(MT_fixture_A$cessation, "0")
  compare_legacy_run_body(MT_output_B$run, MT_fixture_B$run)
  expect_equal(MT_output_B$cessation, "2050")
  expect_equal(MT_fixture_B$cessation, "2050")
})

RS_output_A <- generate_output("RngStream", 1950, 0, outputs_folder)
RS_fixture_A <- get_run_details(test_path("../fixtures/2018/RS/yob_1950_cessation_0.txt"))

RS_output_B <- generate_output("RngStream", 2010, 2050, outputs_folder)
RS_fixture_B <- get_run_details(test_path("../fixtures/2018/RS/yob_2010_cessation_2050.txt"))

test_that("RngStream simulation output in R does not differ from C++ fixtures", {
  skip_if(!is.na(fixture_core) && !identical(fixture_core, pkg_core), fixture_skip_msg)
  compare_legacy_run_body(RS_output_A$run, RS_fixture_A$run)
  expect_equal(RS_output_A$cessation, "0")
  expect_equal(RS_fixture_A$cessation, "0")
  compare_legacy_run_body(RS_output_B$run, RS_fixture_B$run)
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
  # MT vs RngStream can differ more on cessation means for sparse older cohorts at N=1e4
  expect_equal(MT_STATS$mean_cessation, RS_STATS$mean_cessation, tolerance = 1)
  expect_equal(MT_STATS$age_at_death, RS_STATS$age_at_death, tolerance = 0.01)
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
  # Same MT vs RngStream cessation noise as fixed-values comparison above
  expect_equal(MT_STATS_POP$mean_cessation, RS_STATS_POP$mean_cessation, tolerance = 1)
  expect_equal(MT_STATS_POP$age_at_death, RS_STATS_POP$age_at_death, tolerance = 0.01)
})

test_that("Comparison between runSimFromDataFrame and runSimFromFixedValues for MersenneTwister", {
  expect_identical(MT_STATS_POP$mean_initiation, MT_STATS$mean_initiation)
  expect_identical(MT_STATS_POP$mean_cessation, MT_STATS$mean_cessation)
  expect_identical(MT_STATS_POP$age_at_death, MT_STATS$age_at_death)
})
test_that("Comparison between runSimFromDataFrame and runSimFromFixedValues for RngStream", {
  expect_identical(RS_STATS_POP$mean_initiation, RS_STATS$mean_initiation)
  expect_identical(RS_STATS_POP$mean_cessation, RS_STATS$mean_cessation)
  expect_identical(RS_STATS_POP$age_at_death, RS_STATS$age_at_death)
})

test_that("Invalid input configuration path fails with proper error message", {
  input_filepath <- "file_does_not_exist.txt"
  expect_error(shg$LegacyRunWebVersion(input_filepath), "The specified input file 'file_does_not_exist.txt' could not be opened for reading.")
})

test_that("Invalid input parameter path (eg initiation) records error in legacy error file", {
  # RunWebVersion catches SimException and writes <ERROR>...</ERROR> to ERRORFILE; do not rely on
  # Rcpp::warning() alone (Windows/GHA can surface warnings inconsistently vs expect_warning).
  template_path <- readLines(test_path("../templates/test_input_example_incorrect_init_path.txt"))
  input_filepath <- write_input_file_from_template(template_path, "MersenneTwister", 1950, 0, data_folder, outputs_folder)
  err_path <- file.path(outputs_folder, "test_errors_MersenneTwister_1950_0.txt")
  unlink(err_path)

  suppressWarnings(shg$LegacyRunWebVersion(input_filepath))

  expect_true(file.exists(err_path))
  err_txt <- paste(read_output_lines(err_path), collapse = "\n")
  expect_match(err_txt, "[Tt]he specified input file")
  expect_match(err_txt, "initiation_does_not_exist")
  expect_match(err_txt, "LoadProbabilityData")
})

test_that("Invalid output path fails with proper error message", {
  template_path <- readLines(test_path("../templates/test_input_example.txt"))
  outputs_folder <- "folder_does_not_exist"
  input_filepath <- write_input_file_from_template(template_path, "MersenneTwister", 1950, 0, data_folder, outputs_folder)
  # C++ may emit Windows path separators in the error string on Win builders
  expect_error(
    shg$LegacyRunWebVersion(input_filepath),
    regexp = "Specified error file: '.*folder_does_not_exist.*test_errors_MersenneTwister_1950_0\\.txt' could not be opened for writing"
  )
})

# TODO: Compare Legacy tests with runSimFromFixedValues(): requires parsing of results

# Tests for configuration management
test_that("getConfig() with no arguments works (R does not apply C++ default args)", {
  shg <- new(SHGInterface)
  cfg <- shg$getConfig()
  expect_type(cfg, "list")
  expect_equal(cfg$config_version, "1.0")
  cfg_named <- shg$getConfig(debug = FALSE)
  expect_equal(sort(names(cfg)), sort(names(cfg_named)))
})

test_that("factory default mortality_filename is mortality/acm.csv", {
  shg <- new(SHGInterface)
  expect_equal(shg$mortality_filename, "mortality/acm.csv")
  shg$mortality_filename <- "mortality/ocm-excl-lung-cancer.csv"
  shg$reset_to_factory_defaults()
  expect_equal(shg$mortality_filename, "mortality/acm.csv")
})

test_that("getConfig() returns correct structure with config_version", {
  shg_test <- new(SHGInterface)
  config <- shg_test$getConfig(debug = FALSE)
  
  expect_true(is.list(config))
  expect_equal(config$config_version, "1.0")
  expect_true("rng_strategy" %in% names(config))
  expect_true("number_of_segments" %in% names(config))
  expect_true("num_threads" %in% names(config))
  expect_true("seeds" %in% names(config))
  expect_true("input_data_folder" %in% names(config))
  expect_true("mortality_filename" %in% names(config))
  expect_true("params_bundle_source" %in% names(config))
  expect_true("params_mortality" %in% names(config))
  expect_true("cohort_year" %in% names(config))
  expect_true("repeat" %in% names(config))
  expect_true("race" %in% names(config))
  expect_true("sex" %in% names(config))
  expect_true("timestamp" %in% names(config))
})

test_that("getConfig() returns concrete default seeds", {
  shg_rs <- new(SHGInterface)
  shg_rs$rng_strategy <- "RngStream"
  cfg_rs <- shg_rs$getConfig(debug = FALSE)
  expect_equal(cfg_rs$seeds, c(12345, 12345, 12345, 12345, 12345, 12345))

  shg_mt <- new(SHGInterface)
  shg_mt$rng_strategy <- "MersenneTwister"
  cfg_mt <- shg_mt$getConfig(debug = FALSE)
  expect_equal(cfg_mt$seeds, c(1898587603, 1468371936, 1551308340, 1590227640))
})

test_that("getConfig() keeps intent values after simulation", {
  shg_rt <- new(SHGInterface)
  shg_rt$input_data_folder <- data_folder
  shg_rt$rng_strategy <- "RngStream"
  shg_rt$number_of_segments <- -1
  shg_rt$num_threads <- -1
  shg_rt$runSimFromFixedValues(5000, 0, 0, 1950)

  cfg <- shg_rt$getConfig(debug = FALSE)
  expect_equal(cfg$number_of_segments, -1)
  expect_equal(cfg$num_threads, -1)
})

test_that("getReproConfig() captures effective segments but omits num_threads", {
  shg_rt <- new(SHGInterface)
  shg_rt$input_data_folder <- data_folder
  shg_rt$rng_strategy <- "RngStream"
  shg_rt$number_of_segments <- -1
  shg_rt$num_threads <- -1
  shg_rt$runSimFromFixedValues(5000, 0, 0, 1950)

  cfg <- shg_rt$getReproConfig(debug = FALSE)
  expect_true(cfg$number_of_segments >= 1)
  expect_false(identical(cfg$number_of_segments, -1))
  expect_false("num_threads" %in% names(cfg))
  expect_equal(shg_rt$num_threads, -1)
})

test_that("getReproConfig() errors before any completed simulation", {
  shg_rt <- new(SHGInterface)
  expect_error(
    shg_rt$getReproConfig(),
    "No completed simulation is available"
  )
})

test_that("getConfig() records cohort_year for single-cohort runs", {
  shg_rt <- new(SHGInterface)
  shg_rt$input_data_folder <- data_folder
  shg_rt$runSimFromFixedValues(500, 0, 0, 1950)

  cfg <- shg_rt$getConfig(debug = FALSE)
  expect_equal(cfg$cohort_year, 1950)
})

test_that("getConfig() records repeat/race/sex after runSimFromFixedValues", {
  shg_rt <- new(SHGInterface)
  shg_rt$input_data_folder <- data_folder
  shape <- shg_rt$get_data_shape()
  race_value <- as.integer(max(0, shape$num_races - 1))
  shg_rt$runSimFromFixedValues(500, race_value, 0, 1950)

  cfg <- shg_rt$getConfig(debug = FALSE)
  expect_equal(cfg[["repeat"]], 500)
  expect_equal(cfg[["race"]], race_value)
  expect_equal(cfg[["sex"]], 0)
})

test_that("runSimFromFixedValues errors clearly when cohort/race/sex not available", {
  shg_rt <- new(SHGInterface)
  shg_rt$input_data_folder <- data_folder
  shape <- shg_rt$get_data_shape()

  starts <- as.integer(shape$cohort_start_years)
  ends <- as.integer(shape$cohort_end_years)
  min_y <- min(starts, na.rm = TRUE)
  max_y <- max(ends, na.rm = TRUE)
  years <- min_y:max_y
  covered <- rep(FALSE, length(years))
  for (i in seq_along(starts)) {
    covered[years >= starts[i] & years <= ends[i]] <- TRUE
  }
  missing_years <- years[!covered]
  skip_if_not(length(missing_years) > 0, "No cohort gaps in current parameter set.")

  bad_cohort <- as.integer(missing_years[[1]])
  bad_race <- as.integer(shape$num_races)
  bad_sex <- as.integer(shape$num_sexes)
  valid_cohort <- as.integer(starts[[1]])

  out <- "not-assigned"
  expect_error(
    out <- shg_rt$runSimFromFixedValues(10, 0, 0, bad_cohort),
    "Requested cohort_year .* not available"
  )
  expect_identical(out, "not-assigned")
  expect_error(
    shg_rt$runSimFromFixedValues(10, bad_race, 0, valid_cohort),
    "Requested race value .* not available"
  )
  expect_error(
    shg_rt$runSimFromFixedValues(10, 0, bad_sex, valid_cohort),
    "Requested sex value .* not available"
  )
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
  shg1$num_threads <- -1  # auto multi-threaded
  shg1$input_data_folder <- "/test/path"
  shg1$immediate_cessation_year <- 2025
  
  config <- shg1$getConfig(debug = FALSE)
  
  shg2 <- new(SHGInterface)
  shg2$useConfig(config)
  
  expect_equal(shg2$rng_strategy, shg1$rng_strategy)
  expect_equal(shg2$number_of_segments, shg1$number_of_segments)
  expect_equal(shg2$num_threads, shg1$num_threads)
  expect_equal(shg2$input_data_folder, shg1$input_data_folder)
  expect_equal(shg2$immediate_cessation_year, shg1$immediate_cessation_year)
})

test_that("useConfig() clears stale effective runtime cache", {
  shg_rt <- new(SHGInterface)
  shg_rt$input_data_folder <- data_folder
  shg_rt$rng_strategy <- "RngStream"
  shg_rt$number_of_segments <- -1
  shg_rt$num_threads <- -1
  shg_rt$runSimFromFixedValues(500, 0, 0, 1950)

  cfg_repro <- shg_rt$getReproConfig()
  expect_true(cfg_repro$number_of_segments >= 1)

  shg_rt$useConfig(list(
    config_version = "1.0",
    number_of_segments = -1,
    num_threads = -1,
    rng_strategy = "RngStream"
  ))

  cfg_intent <- shg_rt$getConfig()
  expect_equal(cfg_intent$number_of_segments, -1)
  expect_equal(cfg_intent$num_threads, -1)
  expect_error(
    shg_rt$getReproConfig(),
    "No completed simulation is available"
  )
})

test_that("useConfig() validates config_version", {
  shg_test <- new(SHGInterface)
  
  # Missing config_version should still work (assume current format)
  config_no_version <- list(rng_strategy = "RngStream")
  expect_silent(shg_test$useConfig(config_no_version))
  
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

test_that("useConfig() clears stale params provenance when input paths change without bundle keys", {
  shg <- new(SHGInterface)
  shg$params_bundle_source <- "https://example.invalid/bundle.zip"
  shg$params_mortality <- "ocm"
  shg$useConfig(list(
    config_version = "1.0",
    rng_strategy = "RngStream",
    input_data_folder = data_folder
  ))
  cfg <- shg$getConfig()
  expect_true(is.na(cfg$params_bundle_source))
  expect_true(is.na(cfg$params_mortality))
})

test_that("Round-trip: getConfig() -> useConfig() -> verify", {
  shg1 <- new(SHGInterface)
  shg1$num_threads <- 1
  shg1$number_of_segments <- 1
  shg1$rng_strategy <- "MersenneTwister"
  shg1$immediate_cessation_year <- 2020
  
  config <- shg1$getConfig(debug = FALSE)
  
  # Save and reload simulation
  temp_file <- tempfile(fileext = ".rds")
  saveRDS(config, temp_file)
  config_loaded <- readRDS(temp_file)
  
  shg2 <- new(SHGInterface)
  shg2$useConfig(config_loaded)
  cfg2 <- shg2$getConfig(debug = FALSE)
  
  expect_equal(shg2$rng_strategy, shg1$rng_strategy)
  expect_equal(shg2$number_of_segments, shg1$number_of_segments)
  expect_equal(shg2$num_threads, shg1$num_threads)
  expect_equal(shg2$immediate_cessation_year, shg1$immediate_cessation_year)
  expect_equal(cfg2$cohort_year, config_loaded$cohort_year)
  expect_equal(cfg2[["repeat"]], config_loaded[["repeat"]])
  expect_equal(cfg2[["race"]], config_loaded[["race"]])
  expect_equal(cfg2[["sex"]], config_loaded[["sex"]])

  unlink(temp_file)
})

test_that("Constructor with config parameter works", {
  config <- list(
    config_version = "1.0",
    rng_strategy = "RngStream",
    number_of_segments = 4,
    num_threads = -1,  # auto multi-threaded
    immediate_cessation_year = 2025
  )
  
  shg <- new(SHGInterface, config = config)
  
  expect_equal(shg$rng_strategy, "RngStream")
  expect_equal(shg$number_of_segments, 4)
  expect_equal(shg$num_threads, -1)
  expect_equal(shg$immediate_cessation_year, 2025)
})

test_that("Constructor with empty config works", {
  shg <- new(SHGInterface, config = list())
  # Should use defaults
  expect_equal(shg$rng_strategy, "RngStream")
  expect_equal(shg$number_of_segments, -1)  # default is -1 (auto)
})

# Tests for MersenneTwister restrictions
test_that("MersenneTwister cannot be used with multiple segments", {
  shg_test <- new(SHGInterface)
  suppressMessages(shg_test$rng_strategy <- "MersenneTwister")
  expect_error(shg_test$number_of_segments <- 2, "MersenneTwister RNG cannot maintain IID properties with multiple segments")
})

test_that("MersenneTwister cannot be used with multi-threading", {
  shg_test <- new(SHGInterface)
  suppressMessages(shg_test$rng_strategy <- "MersenneTwister")
  expect_error(shg_test$num_threads <- -1, "MersenneTwister RNG requires single-threaded execution")
  expect_error(shg_test$num_threads <- 4, "MersenneTwister RNG requires single-threaded execution")
})

test_that("Switching to MersenneTwister resets segments and threads to valid values", {
  shg_test <- new(SHGInterface)
  shg_test$rng_strategy <- "RngStream"
  shg_test$number_of_segments <- 4
  shg_test$num_threads <- -1  # auto multi-threaded
  
  expect_message(
    expect_message(
      shg_test$rng_strategy <- "MersenneTwister",
      "Resetting number_of_segments to 1"
    ),
    "Resetting num_threads to 1"
  )
  expect_equal(shg_test$number_of_segments, 1)
  expect_equal(shg_test$num_threads, 1)
})

test_that("Multi-threading with single segment warns but allows", {
  shg_test <- new(SHGInterface)
  shg_test$number_of_segments <- 1
  expect_warning(shg_test$num_threads <- -1, "num_threads > 1 or -1 \\(auto\\) has no effect when number_of_segments is 1")
})

test_that("MersenneTwister with multiple segments is reset to 1 segment", {
  shg_test <- new(SHGInterface)
  shg_test$input_data_folder <- data_folder
  # Use RngStream then switch to MersenneTwister
  shg_test$rng_strategy <- "RngStream"
  shg_test$number_of_segments <- 2
  expect_message(
    expect_message(
      shg_test$rng_strategy <- "MersenneTwister",
      "Resetting number_of_segments to 1"
    ),
    "Resetting num_threads to 1"
  )
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

test_that("RngStream allows multiple segments and multi-threading", {
  shg_test <- new(SHGInterface)
  shg_test$rng_strategy <- "RngStream"
  shg_test$number_of_segments <- 4
  shg_test$num_threads <- -1  # auto multi-threaded
  expect_equal(shg_test$rng_strategy, "RngStream")
  expect_equal(shg_test$number_of_segments, 4)
  expect_equal(shg_test$num_threads, -1)
})

test_that("MersenneTwister: custom seeds produce different results and reverting to defaults restores original results", {
  N <- 1000
  shg_mt <- new(SHGInterface)
  shg_mt$input_data_folder <- data_folder
  shg_mt$num_threads <- 1
  shg_mt$number_of_segments <- 1
  shg_mt$rng_strategy <- "MersenneTwister"
  
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
  shg_rs$num_threads <- 1  # single-threaded
  
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
  shg_mt$num_threads <- 1
  shg_mt$number_of_segments <- 1
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
  shg_mt$num_threads <- 1
  shg_mt$number_of_segments <- 1
  shg_mt$rng_strategy <- "MersenneTwister"
  
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
  shg_baseline$num_threads <- 1
  shg_baseline$number_of_segments <- 1
  shg_baseline$rng_strategy <- "MersenneTwister"
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
  shg_rs$num_threads <- 1  # single-threaded
  
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
  shg_mt1$num_threads <- 1
  shg_mt1$number_of_segments <- 1
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

# ============================================================
# CPD Format Tests
# ============================================================

test_that("cpd_format = none produces no CPD column", {
  shg <- new(SHGInterface)
  shg$input_data_folder <- data_folder
  shg$cpd_format <- "none"
  shg$rng_strategy <- "RngStream"
  shg$rngstream_seed <- c(12345, 12345, 12345, 12345, 12345, 12345)
  shg$number_of_segments <- 1
  shg$num_threads <- 1
  
  N <- 100
  df <- test_pop_df(N)
  result <- shg$runSimFromDataFrame(df)
  
  expect_equal(nrow(result), N)
  expect_true("smoking_initiation_age" %in% names(result))
  expect_false("cigarettes_per_day" %in% names(result))
})

test_that("cpd_format = sparse produces compact CPD", {
  shg <- new(SHGInterface)
  shg$input_data_folder <- data_folder
  shg$cpd_format <- "sparse"
  shg$rng_strategy <- "RngStream"
  shg$rngstream_seed <- c(12345, 12345, 12345, 12345, 12345, 12345)
  shg$number_of_segments <- 1
  shg$num_threads <- 1
  
  N <- 100
  df <- test_pop_df(N)
  result <- shg$runSimFromDataFrame(df)
  
  expect_true("cigarettes_per_day" %in% names(result))
  # Sparse format should NOT have parentheses (no age info)
  smoker_idx <- which(result$smoking_initiation_age != -999)[1]
  if (!is.na(smoker_idx)) {
    cpd <- result$cigarettes_per_day[smoker_idx]
    expect_false(grepl("\\(", cpd), info = "Sparse format should not contain parentheses")
  }
})

test_that("cpd_format = legacy produces age-cpd format", {
  shg <- new(SHGInterface)
  shg$input_data_folder <- data_folder
  shg$cpd_format <- "legacy"
  shg$rng_strategy <- "RngStream"
  shg$rngstream_seed <- c(12345, 12345, 12345, 12345, 12345, 12345)
  shg$number_of_segments <- 1
  shg$num_threads <- 1
  
  N <- 100
  df <- test_pop_df(N)
  result <- shg$runSimFromDataFrame(df)
  
  expect_true("cigarettes_per_day" %in% names(result))
  # Legacy format should have parentheses (with age info)
  smoker_idx <- which(result$smoking_initiation_age != -999)[1]
  if (!is.na(smoker_idx)) {
    cpd <- result$cigarettes_per_day[smoker_idx]
    expect_true(grepl("\\(", cpd), info = "Full format should contain parentheses")
  }
})

test_that("cpd_format validation rejects invalid values", {
  shg <- new(SHGInterface)
  expect_error(shg$cpd_format <- "invalid", "cpd_format must be")
})

test_that("sparse and legacy produce equivalent CPD values", {
  N <- 100
  df <- test_pop_df(N)
  
  # Sparse format
  shg_sparse <- new(SHGInterface)
  shg_sparse$input_data_folder <- data_folder
  shg_sparse$cpd_format <- "sparse"
  shg_sparse$rng_strategy <- "RngStream"
  shg_sparse$rngstream_seed <- c(12345, 12345, 12345, 12345, 12345, 12345)
  shg_sparse$number_of_segments <- 1
  shg_sparse$num_threads <- 1
  result_sparse <- shg_sparse$runSimFromDataFrame(df)
  
  # Legacy format (same seed)
  shg_legacy <- new(SHGInterface)
  shg_legacy$input_data_folder <- data_folder
  shg_legacy$cpd_format <- "legacy"
  shg_legacy$rng_strategy <- "RngStream"
  shg_legacy$rngstream_seed <- c(12345, 12345, 12345, 12345, 12345, 12345)
  shg_legacy$number_of_segments <- 1
  shg_legacy$num_threads <- 1
  result_legacy <- shg_legacy$runSimFromDataFrame(df)
  
  # Non-CPD columns should match exactly
  expect_equal(result_sparse$smoking_initiation_age, result_legacy$smoking_initiation_age)
  expect_equal(result_sparse$smoking_cessation_age, result_legacy$smoking_cessation_age)
  expect_equal(result_sparse$age_at_death, result_legacy$age_at_death)
  
  # Extract CPD values from both formats and compare
  for (i in 1:min(10, N)) {
    if (result_sparse$smoking_initiation_age[i] != -999) {
      sparse_vals <- as.numeric(strsplit(result_sparse$cigarettes_per_day[i], ", ")[[1]])
      legacy_vals <- as.numeric(gsub(".*\\(([0-9.]+)\\)", "\\1", 
                                     strsplit(result_legacy$cigarettes_per_day[i], ", ")[[1]]))
      expect_equal(sparse_vals, legacy_vals, info = paste("Individual", i))
    }
  }
})


# ============================================================
# File Output Mode Tests
test_that("Windows: disk output + multi-thread fails before simulation (no output file)", {
  skip_if_not(.Platform$OS.type == "windows")

  output_path <- tempfile(fileext = ".csv")
  on.exit(unlink(output_path), add = TRUE)

  shg <- new(SHGInterface)
  shg$input_data_folder <- data_folder
  shg$rng_strategy <- "RngStream"
  shg$rngstream_seed <- c(12345, 12345, 12345, 12345, 12345, 12345)
  shg$number_of_segments <- 1
  suppressWarnings(shg$num_threads <- -1) # set_num_threads warns when segments==1 (unused threads)
  shg$output_file <- output_path

  df <- test_pop_df(1)

  expect_error(
    shg$runSimFromDataFrame(df),
    "cannot be used with multi-threaded execution"
  )
  expect_false(file.exists(output_path), info = "must stop before creating output")
})

test_that("output_file writes results to disk", {
  output_path <- tempfile(fileext = ".csv")

  shg <- new(SHGInterface)
  shg$input_data_folder <- data_folder
  shg$rng_strategy <- "RngStream"
  shg$rngstream_seed <- c(12345, 12345, 12345, 12345, 12345, 12345)
  shg$number_of_segments <- 2
  # Windows forbids disk + num_threads != 1; Unix test still exercises 2 segments / 2 threads.
  if (.Platform$OS.type == "windows") {
    shg$num_threads <- 1
  } else {
    shg$num_threads <- 2
  }
  shg$output_file <- output_path
  
  N <- 1000
  df <- test_pop_df(N)
  result <- shg$runSimFromDataFrame(df)
  
  # Should return info DataFrame
  expect_true(grepl("file", result$info, ignore.case = TRUE))
  expect_equal(result$rows, N)
  
  # File should exist 
  expect_true(file.exists(output_path))
  lines <- read_output_lines(output_path)
  
  # Find data section (between <RUN> and </RUN>)
  rb <- xml_run_bounds(lines)
  run_start <- rb$start
  run_end <- rb$end
  expect_true(length(run_start) > 0 && length(run_end) > 0, "File should have <RUN> tags")
  data_lines <- lines[(run_start[1]+1):(run_end[1]-1)]
  expect_equal(length(data_lines), N)  # N data lines
  
  # Header should have expected XML structure
  expect_true(any(grepl("<VERSION>", lines)))
  
  unlink(output_path)
})

test_that("runSimFromDataFrame output_file argument writes file without mutating property", {
  output_path <- tempfile(fileext = ".csv")
  on.exit(unlink(output_path), add = TRUE)

  shg <- new(SHGInterface)
  shg$input_data_folder <- data_folder
  shg$rng_strategy <- "RngStream"
  shg$rngstream_seed <- c(12345, 12345, 12345, 12345, 12345, 12345)
  shg$number_of_segments <- 1
  shg$num_threads <- 1
  shg$output_file <- ""

  N <- 200
  df <- test_pop_df(N)
  result <- shg$runSimFromDataFrame(df, output_path)

  expect_true(file.exists(output_path))
  expect_true(grepl("file", result$info, ignore.case = TRUE))
  expect_equal(result$rows, N)
  expect_equal(shg$output_file, "")
})

test_that("output_file parallel execution works (disk + multi-thread, non-Windows)", {
  skip_on_os("windows")

  output_path <- tempfile(fileext = ".csv")
  on.exit(unlink(output_path), add = TRUE)

  shg <- new(SHGInterface)
  shg$input_data_folder <- data_folder
  shg$rng_strategy <- "RngStream"
  shg$rngstream_seed <- c(12345, 12345, 12345, 12345, 12345, 12345)
  shg$number_of_segments <- 10
  shg$num_threads <- -1
  shg$output_file <- output_path

  N <- 10000
  df <- test_pop_df(N)

  shg$runSimFromDataFrame(df)
  lines <- read_output_lines(output_path)
  rb <- xml_run_bounds(lines)
  run_start <- rb$start
  run_end <- rb$end
  expect_true(length(run_start) > 0 && length(run_end) > 0)
  data_lines <- lines[(run_start[1] + 1):(run_end[1] - 1)]
  expect_equal(length(data_lines), N)
})

test_that("output_file produces same init/cess/ocd as memory mode", {
  output_path <- tempfile(fileext = ".csv")
  
  N <- 500
  df <- test_pop_df(N)
  
  # Memory mode
  shg_mem <- new(SHGInterface)
  shg_mem$input_data_folder <- data_folder
  shg_mem$rng_strategy <- "RngStream"
  shg_mem$rngstream_seed <- c(12345, 12345, 12345, 12345, 12345, 12345)
  shg_mem$cpd_format <- "legacy"
  shg_mem$number_of_segments <- 1
  shg_mem$num_threads <- 1
  result_mem <- shg_mem$runSimFromDataFrame(df)
  
  # File mode (same seed)
  shg_file <- new(SHGInterface)
  shg_file$input_data_folder <- data_folder
  shg_file$rng_strategy <- "RngStream"
  shg_file$rngstream_seed <- c(12345, 12345, 12345, 12345, 12345, 12345)
  shg_file$number_of_segments <- 1
  shg_file$num_threads <- 1
  shg_file$output_file <- output_path
  shg_file$runSimFromDataFrame(df)
  
  # Parse file and compare (skip XML header, find <RUN> tag to get data lines)
  lines <- read_output_lines(output_path)
  rb <- xml_run_bounds(lines)
  run_start <- rb$start
  run_end <- rb$end
  if (length(run_start) > 0 && length(run_end) > 0) {
    data_lines <- lines[(run_start[1]+1):(run_end[1]-1)]
  } else {
    # Fallback for old format (simple header)
    data_lines <- lines[-1]
  }
  file_init_ages <- sapply(data_lines, function(line) {
    as.integer(strsplit(line, ";")[[1]][4])
  })
  
  expect_equal(unname(file_init_ages), result_mem$smoking_initiation_age)
  
  unlink(output_path)
})

test_that("NHIS csv-partial bundle lists expected filenames (checked in test-nhis-partial-inputs.R)", {
  nh <- test_path("../testdata/NHIS-1965-2018/csv-partial")
  skip_if_not(dir.exists(nh), "NHIS csv-partial fixtures missing (not in CRAN tarball; use full git checkout)")
  req <- c(
    "initiation.csv", "cessation.csv", "cpd.csv", "acm.csv", "ocm-excl-lung-cancer.csv"
  )
  for (f in req) {
    expect_true(file.exists(file.path(nh, f)), info = f)
  }
})
