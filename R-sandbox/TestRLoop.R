devtools::load_all()
library(RcppSmokingHistoryGenerator)
shg <-new(SHGInterface)

library(parallel)

N <- 10000
pop2 <- list(
  race = rep(0, N),
  sex = sample(x = c(0, 1), size = N, prob = c(0.5, 0.5), replace = TRUE),
  birth_cohort = rep(1930:1949, N / 20)
)

# runSimulationsFromPopulation <- function(pop) {
#   results <- lapply(1:length(pop$race), function(i) {
#     shg$runSim(
#       1,
#       pop$race[i],
#       pop$sex[i],
#       pop$birth_cohort[i]
#     )
#   })
#   do.call(rbind, results)
# }

# runSimulationsFromPopulation <- function(pop) {
#   num_cores <- 8 #detectCores() - 1
#   results <- mclapply(1:length(pop$race), function(i) {
#     shg$runSim(
#       1,
#       pop$race[i],
#       pop$sex[i],
#       pop$birth_cohort[i]
#     )
#   }, mc.cores = num_cores)
#   do.call(rbind, results)
# }

split_list <- function(pop, num_chunks) {
  split_indices <- split(1:N, cut(1:N, num_chunks, labels = FALSE))
  print(split_indices[[1]])
  lapply(split_indices, function(indices) {
    list(
      race = pop$race[indices],
      sex = pop$sex[indices],
      birth_cohort = pop$birth_cohort[indices],
      sex_numeric = pop$sex_numeric[indices]
    )
  })
}

runSimulationsFromPopulation <- function(pop) {
  num_cores <- 8 #detectCores() - 1
  pop_chunks <- split_list(pop, num_cores)
  results <- mclapply(pop_chunks, function(chunk) {
    lapply(1:length(chunk$race), function(i) {
      shg$runSim(
        1,
        chunk$race[i],
        chunk$sex[i],
        chunk$birth_cohort[i]
      )
    })
  }, mc.cores = num_cores)
  do.call(rbind, unlist(results, recursive = FALSE))
}


system.time({
  test <- runSimulationsFromPopulation(pop2)
})
