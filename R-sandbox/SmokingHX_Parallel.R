library(rbenchmark)
library(smokingHxGen)
N=10^6

# Define a function to run the code
run_code <- function() {
  start_time <- Sys.time()
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
  print(end_time - start_time)
    return(SHx)
}

# Run the code in parallel
library(parallel)
num_cores <- 10 #detectCores()
cl <- makeCluster(num_cores)
clusterEvalQ(cl, {
  library(rbenchmark)
  library(smokingHxGen)
})
clusterExport(cl, "run_code")
clusterExport(cl, "N")

system.time({
  results <- parLapply(cl, 1:40, function(i) {
    run_code()
  })
  stopCluster(cl)
})

# Access the results
# results[[1]] contains the result of the first run
# results[[2]] contains the result of the second run
# and so on...

