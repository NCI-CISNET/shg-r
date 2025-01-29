library(rbenchmark)

# Define a function to run the code
run_code <- function() {
  curdir = getwd()
  setwd("/Users/jclarke/Documents/GitHub/smoking-history-generator/")
  system("/Users/jclarke/Documents/GitHub/smoking-history-generator/bin/lbc_smokehist_osx.exe /Users/jclarke/Documents/GitHub/smoking-history-generator/test_input.txt")
  setwd(curdir)
  return(TRUE)
}

# Run the code in parallel
library(parallel)
num_cores <- 10 #detectCores()
cl <- makeCluster(num_cores)
clusterEvalQ(cl, {
  library(rbenchmark)
  #library(smokingHxGen)
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

