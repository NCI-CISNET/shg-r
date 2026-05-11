shg_cli_path <- "/path-to-shg-cli/"

#Sys.setenv(PKG_BUILD_EXTRA_FLAGS = "false")
devtools::clean_dll()
# Note: debug = TRUE results in "(debug build)" and -O0 -g etc. which overrides Makevars
pkgbuild::compile_dll(path = ".", debug = FALSE)

#devtools::load_all()
devtools::load_all()
library(SmokingHistoryGenerator)
shg <- new(SHGInterface)

wd <- getwd()
setwd(tempdir())
d <- system.file("extdata", package = "SmokingHistoryGenerator")
tf <- tempfile("shg_legacy_", fileext = ".txt")
writeLines(
  c(
    "RNGSTRATEGY=MersenneTwister",
    "SEED_INIT=12345", "SEED_CESS=12345", "SEED_MORTALITY=12345", "SEED_MISC=12345",
    "RACE=0", "SEX=0", "YOB=1950", "CESSATION_YR=0", "REPEAT=100",
    paste0("INIT_PROB=", file.path(d, "smoking", "initiation.csv")),
    paste0("CESS_PROB=", file.path(d, "smoking", "cessation.csv")),
    paste0("MORTALITY_PROB=", file.path(d, "mortality", "ocm-excl-lung-cancer.csv")),
    paste0("CPD_DATA=", file.path(d, "smoking", "cpd.csv")),
    paste0("OUTPUTFILE=", tempfile("mt_out_", fileext = ".txt")),
    paste0("ERRORFILE=", tempfile("mt_err_", fileext = ".txt"))
  ),
  tf
)
shg$LegacyRunWebVersion(tf)
setwd(wd)

# SHG legacy version; results should be the same as above.
# start_time <- Sys.time()
# curdir <- getwd()
# setwd(shg_cli_path)
# system("./bin/lbc_smokehist_osx_6.3.3.exe test_input.txt")
# setwd(curdir)
# end_time <- Sys.time()
# print(end_time - start_time)


# Test very small population to determine overhead
N <- 100
start_time <- Sys.time()
shg$number_of_segments <- 10
shg$num_threads <- 1
shg$rng_strategy <- "RngStream"
#shg$rng_strategy <- "MersenneTwister"
RNGSTREAM_SIM <- shg$runSimFromFixedValues(N, 0, 0, 1940)
end_time <- Sys.time()
print(end_time - start_time)


N <- 10^6
start_time <- Sys.time()
shg$number_of_segments <- 10
shg$num_threads <- -1
shg$rng_strategy <- "RngStream"
shg$immediate_cessation_year <- 2010

#should fail
shg$cessation_filename <- "cessation_does_not_exist.csv"

RNGSTREAM_SIM <- shg$runSimFromFixedValues(N, 0, 0, 1940)
end_time <- Sys.time()
print(end_time - start_time)
#TODO memory usage increases with every run...

# Simulate the SHG legacy version in order to compare the results (which should be identical to the CLI)
N <- 10^6
start_time <- Sys.time()
shg$number_of_segments <- 10
shg$num_threads <- -1
shg$rng_strategy <- "MersenneTwister"
MT_SIM <- shg$runSimFromFixedValues(N, 0, 0, 1950)
end_time <- Sys.time()
print(end_time - start_time)

N <- 10^6
start_time <- Sys.time()
pop <- list(
    race = rep(0, N),
    sex = sample(x = c(0, 1), size = N, prob = c(0.5, 0.5), replace = TRUE),
    birth_cohort = rep(1930:1949, N / 20)
)
shg$rng_strategy <- "RngStream"
shg$number_of_segments <- 10
shg$num_threads <- -1

test <- shg$runSimFromDataFrame(pop)

end_time <- Sys.time()
print(end_time - start_time)

# df <- as.data.frame(test)
# library(data.table)
# MT_SIM_DT <- as.data.table(df)
# end_time <- Sys.time()
# print(end_time - start_time)

# SHG legacy version; results should be the same as above.
# start_time <- Sys.time()
# curdir <- getwd()
# setwd(shg_cli_path)
# system("./bin/lbc_smokehist_osx_6.3.3.exe test_input.txt")
# setwd(curdir)
# end_time <- Sys.time()
# print(end_time - start_time)


# # Test that the output from MT_SIM is the same as the output from the legacy version
# source("./tools/parseLegacyOutput.R")

# column_names <- colnames(MT_SIM)

# # Parse the text file and extract the data
file_path <- file.path(shg_cli_path, "test_output.out")
# parsed_data <- parse_run_data(file_path, column_names)
# #testthat::expect_equal(parsed_data, MT_SIM_TRUNC)

# # Load the diffdf package
# library(diffdf)

# # Compare the data frames and visualize the differences

# random_rows <- c(1, nrow(MT_SIM), sample(1:nrow(MT_SIM), 100))

# diff_result <- diffdf(parsed_data[random_rows, ], MT_SIM[random_rows, ])

# # Print the differences
# print(diff_result)

# source("./tools/Tests.r")

# N <- 10^6
# library(smokingHxGen)
# start_time <- Sys.time()
# pop <-
# list(
#     race = rep("white", N),
#     sex = sample(x = c("male", "female"), size = N, prob = c(0.5, 0.5), replace = TRUE),
#     birth_cohort = rep(1930:1949, N / 20),
#     some_other_var = stats::runif(N)
# )

# shg_bladder <- SmokingHistoryGenerator$new(
# population = pop,
# cause_of_death = "all_causes",
# smoking_history_simulation_start_age = getOption("shg.min_smoking_initiation_age"), # 8
# smoking_history_simulation_stop_age = getOption("shg.max_age"), # 110
# lifetime_simulation_spawn_ages = 40,
# lifetime_simulation_stop_ages = getOption("shg.max_age"),
# specifications = getOption("shg.constants"), # coefficients
# validate = TRUE
# )
# SHx <- shg_bladder$get_smoking_exposure_history(copy = TRUE)
# end_time <- Sys.time()
# print(end_time - start_time)


#shg <- new(SHGInterface)
#shg$runSim(N, 0, 0, 1945)
#shg$runSim(N, 0, 0, 1950)

#shg$LegacyRunWebVersion("/inst/inputs/test_input.txt")
