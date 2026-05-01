#!/usr/bin/env Rscript
# Benchmark script for SmokingHistoryGenerator
# Tests performance generating 1 million smoking histories
#
# Usage:
#   Rscript tools/benchmark-1M.R
#
# Optional: Install from feature branch first
#   Rscript tools/benchmark-1M.R --install

args <- commandArgs(trailingOnly = TRUE)

# Install from feature branch if requested
if ("--install" %in% args) {
  cat("=== Installing from feature/v6.5.0-sync ===\n\n")
  if (!requireNamespace("devtools", quietly = TRUE)) {
    install.packages("devtools")
  }
  devtools::install_github("NCI-CISNET/shg-r@feature/v6.5.0-sync", force = TRUE)
}

library(SmokingHistoryGenerator)

cat("==========================================================\n")
cat("  SmokingHistoryGenerator Benchmark (1 Million Records)\n")
cat("==========================================================\n\n")

cat("Package version:", as.character(packageVersion("SmokingHistoryGenerator")), "\n")
cat("R version:", R.version.string, "\n")
cat("Platform:", R.version$platform, "\n")
cat("CPU cores:", parallel::detectCores(), "\n\n")

N <- 1000000  # 1 million individuals
RUNS <- 3     # Number of benchmark runs

# -----------------------------------------------------------------------------
# Benchmark function
# -----------------------------------------------------------------------------
run_benchmark <- function(name, setup_fn, runs = RUNS) {
  cat(sprintf("--- %s ---\n", name))

  times <- numeric(runs)
  for (i in seq_len(runs)) {
    shg <- setup_fn()

    gc()  # Clean up before timing
    t <- system.time({
      result <- shg$runSimFromFixedValues(N, race = 0, sex = 0, cohort_year = 1950)
    })
    times[i] <- t["elapsed"]

    cat(sprintf("  Run %d: %.2f sec (%.0f records/sec)\n",
                i, times[i], N / times[i]))
  }

  cat(sprintf("  Mean: %.2f sec | Min: %.2f sec | Max: %.2f sec\n",
              mean(times), min(times), max(times)))
  cat(sprintf("  Throughput: %.0f records/sec\n\n", N / mean(times)))

  return(times)
}

# -----------------------------------------------------------------------------
# Benchmark 1: RngStream with auto-parallelism (recommended)
# -----------------------------------------------------------------------------
results <- list()

results$rngstream_auto <- run_benchmark(
  "RngStream (auto threads, auto segments)",
  function() {
    shg <- new(SHGInterface)
    shg$rng_strategy <- "RngStream"
    shg$num_threads <- -1
    shg$number_of_segments <- -1
    shg
  }
)

# -----------------------------------------------------------------------------
# Benchmark 2: RngStream single-threaded (baseline)
# -----------------------------------------------------------------------------
results$rngstream_single <- run_benchmark(
  "RngStream (1 thread, 1 segment)",
  function() {
    shg <- new(SHGInterface)
    shg$rng_strategy <- "RngStream"
    shg$num_threads <- 1
    shg$number_of_segments <- 1
    shg
  }
)


# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------
cat("==========================================================\n")
cat("  SUMMARY\n")
cat("==========================================================\n\n")

summary_df <- data.frame(
  Configuration = c(
    "RngStream (parallel)",
    "RngStream (single)"
  ),
  Mean_Sec = c(
    mean(results$rngstream_auto),
    mean(results$rngstream_single)
  ),
  Records_Per_Sec = c(
    N / mean(results$rngstream_auto),
    N / mean(results$rngstream_single)
  )
)

baseline_rps <- summary_df$Records_Per_Sec[nrow(summary_df)]
summary_df$Speedup <- summary_df$Records_Per_Sec / baseline_rps

print(summary_df, row.names = FALSE)

cat("\n")
cat(sprintf("Parallel speedup: %.1fx faster than single-threaded\n",
            mean(results$rngstream_single) / mean(results$rngstream_auto)))

cat("\nNote: For additional ~5-20%% performance, add to ~/.R/Makevars:\n")
cat("  CXX17FLAGS += -march=native\n")

