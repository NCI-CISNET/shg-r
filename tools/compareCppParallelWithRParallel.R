Sys.setenv(PKG_BUILD_EXTRA_FLAGS = "false")
devtools::clean_dll()
devtools::load_all()
devtools::load_all()
library(rbenchmark)
library(RcppSmokingHistoryGenerator)
library(parallel)
library(dplyr)  # For bind_rows to combine DataFrames

N <- 10^6
cores <- 10

results1 <- benchmark(
  R_STREAM_SINGLE_SEQUENTIAL = {
    shg <- new(SHGInterface)
    shg$number_of_segments <- 1
    shg$run_multi_threaded <- FALSE
    shg$rng_strategy <- "RngStream"
    STREAM_SIM_SEQUENTIAL <- shg$runSim(N, 0, 0, 1940)
  },
  R_STREAM_SINGLE_PARALLEL = {
    shg <- new(SHGInterface)
    shg$number_of_segments <- cores
    shg$run_multi_threaded <- TRUE
    shg$rng_strategy <- "RngStream"
    STREAM_SIM <- shg$runSim(N, 0, 0, 1940)
  },
  R_STREAM_MCLAPPLY_PARALLEL = {
    num_simulations <- N/cores
    shg_list <- lapply(1:cores, function(x) {
      shg <- new(SHGInterface) # Class
      shg$number_of_segments <- 1
      shg$run_multi_threaded <- FALSE
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
  replications = 1 # Number of times to run each expression
)

results2 <- benchmark(
  CLI_SINGLE_SEQUENTIAL = {
    curdir <- getwd()
    setwd("/Users/jclarke/Documents/GitHub/smoking-history-generator/")
    system("./bin/lbc_smokehist_osx.exe test_input.txt")
    setwd(curdir)
  },
  CLI_MCLAPPLY_PARALLEL = {
    curdir <- getwd()
    setwd("/Users/jclarke/Documents/GitHub/smoking-history-generator/")
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

