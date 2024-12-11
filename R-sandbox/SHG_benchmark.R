library(Rcpp)
library(rbenchmark)
library(smokingHxGen)
#devtools::load_all() # recompiles the package if necessary

Sys.setenv(PKG_BUILD_EXTRA_FLAGS = "false")
devtools::clean_dll()
devtools::load_all()

library(RcppSmokingHistoryGenerator)  # Package name
# shg <- new(SHGInterface) # Class

# shg$setRNGtype("MT") # Use internal (non-R) Mersenne Twister
# Hx_MT <- shg$runSim(N, 0, 0, 1940) # should be different (continues with same streams)

# shg$setRNGtype("rstream") # use MRG32k3a with substreams and batch generation
# Hx_RSTREAM <- shg$runSim(N, 0, 0, 1940) # should be same as first instance of test

# Benchmark the different approaches
N <- 10^6
counter <- 0
results <- benchmark(
  SIM_RSTREAM_PARALLEL = {
    counter <- counter + 1
    cat('Running SIM_RSTREAM_PARALLEL, count:', counter, '\n')
    shg <- new(SHGInterface) # Class
    RSTREAM_SIM <- shg$runSim(N, 0, 0, 1950)
  },
  SIM_MT = {
    #shg$setRNGtype("MT")
    shg <- new(SHGInterface) # Class
    MT_SIM <- shg$runSim(N, 0, 0, 1940)
  },
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
  # SHG_CLI_6.3.4_SingleThread = {
  #   curdir <- getwd()
  #   setwd("/Users/jclarke/Documents/GitHub/smoking-history-generator/")
  #   system("./bin/lbc_smokehist_osx.exe test_input.txt")
  #   setwd(curdir)
  # },
  # SHG_CLI_6.3.3_SingleThread = {
  #   curdir <- getwd()
  #   setwd("/Users/jclarke/Documents/GitHub/smoking-history-generator/")
  #   system("./bin/lbc_smokehist_osx_6.3.3.exe test_input.txt")
  #   setwd(curdir)
  # },
  order = "relative",
  replications = 1 # Number of times to run each expression
)

# Print out the benchmark results
print(results)
#results <- results[, c("test", "elapsed", "relative")]
#print(results)



