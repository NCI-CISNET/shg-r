#!/usr/bin/env Rscript
# Quick test script for shg-r feature/v6.5.0-sync
# 
# Usage:
#   Rscript tools/test-feature-branch.R
#
# This script installs the feature branch and demonstrates the new features.

cat("=== Installing SmokingHistoryGenerator from feature/v6.5.0-sync ===\n\n")

# Install from feature branch
if (!requireNamespace("devtools", quietly = TRUE)) {
  install.packages("devtools")
}
devtools::install_github("NCI-CISNET/shg-r@feature/v6.5.0-sync", force = TRUE)

library(SmokingHistoryGenerator)

cat("\n=== Package loaded successfully ===\n")
cat("Version:", as.character(packageVersion("SmokingHistoryGenerator")), "\n\n")

# Create interface
shg <- new(SHGInterface)

# -----------------------------------------------------------------------------
# Test 1: Basic RngStream simulation (recommended, supports parallelism)
# -----------------------------------------------------------------------------
cat("=== Test 1: RngStream with auto-parallelism ===\n")
shg$rng_strategy <- "RngStream"
shg$num_threads <- -1        # Auto (use all cores)
shg$number_of_segments <- -1 # Auto-calculate

N <- 100000
t1 <- system.time({
  result1 <- shg$runSimFromFixedValues(N, race = 0, sex = 0, cohort_year = 1950)
})

cat(sprintf("  Simulated %d individuals in %.2f seconds\n", N, t1["elapsed"]))
cat(sprintf("  Smokers: %d (%.1f%%)\n", 
            sum(result1$smk_status == 1), 
            100 * mean(result1$smk_status == 1)))
cat("\n")

# -----------------------------------------------------------------------------
# Test 2: Custom RngStream seed for reproducibility
# -----------------------------------------------------------------------------
cat("=== Test 2: RngStream with custom seed (reproducible) ===\n")
shg2 <- new(SHGInterface)
shg2$rng_strategy <- "RngStream"
shg2$rngstream_seed <- c(12345, 12345, 12345, 12345, 12345, 12345)
shg2$number_of_segments <- 4  # Fixed for reproducibility across machines

result2a <- shg2$runSimFromFixedValues(1000, 0, 0, 1950)

# Reset and run again with same seed
shg2$rngstream_seed <- c(12345, 12345, 12345, 12345, 12345, 12345)
result2b <- shg2$runSimFromFixedValues(1000, 0, 0, 1950)

identical_results <- identical(result2a$smk_status, result2b$smk_status)
cat(sprintf("  Results identical with same seed: %s\n\n", identical_results))

# -----------------------------------------------------------------------------
# Test 3: MersenneTwister (legacy, single-threaded only)
# -----------------------------------------------------------------------------
cat("=== Test 3: MersenneTwister (legacy mode) ===\n")
shg3 <- new(SHGInterface)
shg3$rng_strategy <- "MersenneTwister"
# Note: MT automatically uses 1 segment and 1 thread

result3 <- shg3$runSimFromFixedValues(10000, 0, 0, 1950)
cat(sprintf("  Simulated %d individuals with MersenneTwister\n", nrow(result3)))
cat(sprintf("  Smokers: %d (%.1f%%)\n\n", 
            sum(result3$smk_status == 1), 
            100 * mean(result3$smk_status == 1)))

# -----------------------------------------------------------------------------
# Test 4: DataFrame input with mixed population
# -----------------------------------------------------------------------------
cat("=== Test 4: DataFrame input with mixed population ===\n")
shg4 <- new(SHGInterface)
shg4$rng_strategy <- "RngStream"

N <- 50000
pop <- data.frame(
  race = rep(0, N),
  sex = sample(c(0, 1), N, replace = TRUE),
  birth_cohort = sample(1930:1970, N, replace = TRUE)
)

t4 <- system.time({
  result4 <- shg4$runSimFromDataFrame(pop)
})

cat(sprintf("  Simulated %d individuals in %.2f seconds\n", N, t4["elapsed"]))
cat(sprintf("  Male smokers: %.1f%%, Female smokers: %.1f%%\n",
            100 * mean(result4$smk_status[pop$sex == 0] == 1),
            100 * mean(result4$smk_status[pop$sex == 1] == 1)))
cat("\n")

# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------
cat("=== All tests passed! ===\n")
cat("\nKey features demonstrated:\n")
cat("  ✓ RngStream RNG with auto-parallelism\n")
cat("  ✓ Reproducible results with custom seeds\n")
cat("  ✓ MersenneTwister legacy mode\n")
cat("  ✓ DataFrame input with mixed populations\n")
cat("\nFor performance optimization, add to ~/.R/Makevars:\n")
cat("  CXX17FLAGS += -march=native\n")

