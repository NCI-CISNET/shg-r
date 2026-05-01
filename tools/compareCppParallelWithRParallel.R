Sys.setenv(PKG_BUILD_EXTRA_FLAGS = "false")
devtools::clean_dll()
devtools::load_all()
devtools::load_all()
library(rbenchmark)
library(SmokingHistoryGenerator)
library(parallel)
library(dplyr)  # For bind_rows to combine DataFrames

N <- 10^6
cores <- 10
shg_cli_path <- "/path-to-shg-cli/"

results1 <- benchmark(
  R_STREAM_SINGLE_SEQUENTIAL = {
    shg <- new(SHGInterface)
    shg$number_of_segments <- 1
    shg$num_threads <- 1
    shg$rng_strategy <- "RngStream"
    STREAM_SIM_SEQUENTIAL <- shg$runSim(N, 0, 0, 1940)
  },
  R_STREAM_SINGLE_PARALLEL = {
    shg <- new(SHGInterface)
    shg$number_of_segments <- cores
    shg$num_threads <- -1
    shg$rng_strategy <- "RngStream"
    STREAM_SIM <- shg$runSim(N, 0, 0, 1940)
  },
  R_STREAM_MCLAPPLY_PARALLEL = {
    num_simulations <- N/cores
    shg_list <- lapply(1:cores, function(x) {
      shg <- new(SHGInterface) # Class
      shg$number_of_segments <- 1
      shg$num_threads <- 1
      shg$rng_strategy <- "RngStream"
      return(shg)
    })
    result_list <- mclapply(shg_list, function(shg) {
      shg$runSim(num_simulations, 0, 0, 1940)
    }, mc.cores = cores)
    # Combine the results into a single DataFrame
    STREAM_SIM_R <- bind_rows(result_list)
  },
  order = "relative",
  smokingHxGen = {
    pop <-
      list(
        race = rep("white", N),
        sex = sample(x = c("male", "female"), size = N, prob = c(0.5, 0.5), replace = TRUE),
        birth_cohort = rep(1930:1949, N / 20),
        some_other_var = stats::runif(N)
      )

    shg_bladder <- SmokingHistoryGenerator$new(
      population = pop,
      cause_of_death = "all_causes",
      smoking_history_simulation_start_age = getOption("shg.min_smoking_initiation_age"), # 8
      smoking_history_simulation_stop_age = getOption("shg.max_age"), # 110
      lifetime_simulation_spawn_ages = 40,
      lifetime_simulation_stop_ages = getOption("shg.max_age"),
      specifications = getOption("shg.constants"), # coefficients
      validate = TRUE
    )
    SHx <- shg_bladder$get_smoking_exposure_history(copy = FALSE)
  },
    order = "relative",
  replications = 1 # Number of times to run each expression
)

results2 <- benchmark(
  CLI_SINGLE_SEQUENTIAL = {
    curdir <- getwd()
    setwd(shg_cli_path)
    system("./bin/lbc_smokehist_osx.exe test_input.txt")
    setwd(curdir)
  },
  CLI_MCLAPPLY_PARALLEL = {
    curdir <- getwd()
    setwd(shg_cli_path)
    result_list <- mclapply(1:10, function(x) {
      system("./bin/lbc_smokehist_osx.exe test_input_10_5.txt")
    }, mc.cores = 10)
    setwd(curdir)
  },
  order = "relative",
  replications = 1 # Number of times to run each expression
)

print(results1)
print(results2)

