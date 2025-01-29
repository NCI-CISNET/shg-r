library(Rcpp)
library(rstream)
library(rbenchmark)
#Rcpp::sourceCpp("./src/wrapper.cpp")

# library(Rcpp)
devtools::load_all()
devtools::load_all()
library(RcppSmokingHistoryGenerator)  # Package name
shg <- new(SHGInterface) # Class
rng1 <- new("rstream.mrg32k3a")
rng2 <- rstream.clone(rng1)
rstream.nextsubstream(rng2)
rng3 <- rstream.clone(rng2)
rstream.nextsubstream(rng3)
rng4 <- rstream.clone(rng3)
rstream.nextsubstream(rng4)

shg$initialize() # instantiates the Smoking_Simulator object in the interface

shg$setRNGs(rng1, rng2, rng3, rng4) # after initialize so that they can hang off the object

N <- 10^6

#
# test = shg$GetNextInitRand()
# n <- 10
# random_numbers <- replicate(n, shg$GetNextInitRand())

# Function to generate random numbers using different methods
# generate_random_numbers <- function() {
#   n <- 1000  # Number of random numbers to generate
#
#   # # Method 1: for loop
#   # random_numbers_loop <- numeric(n)
#   # for (i in 1:n) {
#   #   random_numbers_loop[i] <- shg$GetNextInitRand()
#   # }
#   #
#   # # Method 2: sapply
#   # random_numbers_sapply <- sapply(1:n, function(x) shg$GetNextInitRand())
#
#   # Method 1: Internal MT
#   # random_numbers_replicate <- replicate(n, shg$GetNextInitRand())
#   #
#   # # Method 1: Internal MT
#   # random_numbers_replicate_R <- replicate(n, shg$GetNextInitRand_R())
#
#   return(list(loop = random_numbers_loop, sapply = random_numbers_sapply, replicate = random_numbers_replicate))
# }

test3 <- shg$GetNextCessRand_R_vector(N)

# Benchmark the different approaches

results <- benchmark(
  RCPP_MT = {
    random_numbers_replicate1 <- replicate(N, shg$GetNextInitRand())
  },
  RCPP_rstream_passthrough = {
    random_numbers_replicate2 <- replicate(N, shg$GetNextCessRand_R())
  },
  RCPP_rstream_passthrough_no_switching = {
    random_numbers_replicate2 <- shg$GetNextCessRand_R_vector(N)
  },
  R_rstream = {
    random_numbers_replicate2 <- rstream.sample(rng1, N)
  },
  R_runif = {
    random_numbers_replicate2 <- runif(N)
  },
  order = "relative",
  replications = 3 # Number of times to run each expression
)

results <- results[, c("test", "elapsed", "relative")]

# Print out the benchmark results
print(results)

# MRG is about 1.458 MT (so even without the switching, it will cost more * 4 streams)
# Guessing that RNG is only about 10% of the total sim cost;

#test <- shg$runSim(10000)
#10K with MRG = 4.26 secs
#10K with MT = 0.426 secs

#41.75??? secs with MT; 4.17 with MRG

#N <- 10^5 ~3.7 secs with CLI
#N <- 10^6 ~3.8 secs with CLI

N <- 10^5
#N <- 10^5 ~4 secs with MT
#N <- 10^6 ~41 secs with MT
start_time <- Sys.time()
shg$setRNGtype("MT")
MT_SIM <- shg$runSim(N)
end_time <- Sys.time()
end_time - start_time

#N <- 10^5 ~4.5 secs with RSTREAM
#N <- 10^6 ~45 secs with RSTREAM
start_time <- Sys.time()
shg$setRNGtype("rstream")
RSTREAM_SIM <- shg$runSim(N)
end_time <- Sys.time()
end_time - start_time


start_time <- Sys.time()
N <- 10^6

#N <- 10^5 ~2.28 secs for Bladder Hx model
#N <- 10^6 ~18 secs for Bladder Hx model

#N <- 10^5 ~0.86 secs for RSTREAM in parallel
#N <- 10^6 7.23 secs for RSTREAM in parallel


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
SHx <- shg_bladder$get_smoking_exposure_history(copy = TRUE)

end_time <- Sys.time()
end_time - start_time

