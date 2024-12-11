library(Rcpp)
library(rbenchmark)
library(parallel)
library(dplyr)  # For bind_rows to combine DataFrames

Sys.setenv(PKG_BUILD_EXTRA_FLAGS = "false")
devtools::clean_dll()
devtools::load_all()

library(RcppSmokingHistoryGenerator)  # Package name

shg <- new(SHGInterface)
N <- 10^6
start_time <- Sys.time()
MT_SIM = shg$runSim(N, 0, 0, 1940)
end_time <- Sys.time()
print(end_time - start_time)
# detectCores() - 2 # Adjust the number of cores as needed

start_time <- Sys.time()

cores <- 10  # Define the number of cores you want to use
N <- 10^6
num_simulations <- N/cores

shg_list <- lapply(1:cores, function(x) {
  shg <- new(SHGInterface) # Class
  #MT_SIM <- shg$runSim(num_simulations, 0, 0, 1940)
  return(shg)
})

result_list <- mclapply(shg_list, function(shg) {
  shg$runSim(num_simulations, 0, 0, 1940)
}, mc.cores = cores)

# Combine the results into a single DataFrame
Hx_RSTREAM <- bind_rows(result_list)
end_time <- Sys.time()
end_time - start_time


cores <- 10  # Define the number of cores you want to use
N <- 10^6
num_simulations <- N/cores
result_list <- mclapply(shg_list, function(shg) {
  shg <- new(SHGInterface) # Class
}, mc.cores = cores)

# Define a function to apply
my_function <- function(shg) {
  shg$runSim(num_simulations, 0, 0, 1940)
}

# Apply the function to the list in parallel using 2 cores
result <- mclapply(shg_list, my_function, mc.cores = 10)

# Print the result
combined_result <- bind_rows(result)

