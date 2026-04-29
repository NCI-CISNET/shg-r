#!/usr/bin/env Rscript
# Local R CMD check script for SmokingHistoryGenerator
# Run this before committing to catch issues early
#
# Usage:
#   Rscript tools/check-package.R
#   # or from R:
#   source("tools/check-package.R")

cat("Running R CMD check for SmokingHistoryGenerator...\n\n")

# Check if we're in the right directory
if (!file.exists("DESCRIPTION")) {
  stop("Please run this script from the shg-r package root (directory containing DESCRIPTION)")
}

# Run check with rcmdcheck if available, otherwise use base R CMD check
if (requireNamespace("rcmdcheck", quietly = TRUE)) {
  result <- rcmdcheck::rcmdcheck(
    args = c("--no-manual", "--as-cran"),
    build_args = c("--no-build-vignettes"),
    error_on = "warning"
  )
} else {
  cat("rcmdcheck package not found. Using base R CMD check.\n")
  cat("For better output, install rcmdcheck: install.packages('rcmdcheck')\n\n")
  
  # Build the package
  system("R CMD build . --no-manual --no-build-vignettes")
  
  # Find the tarball
  tarball <- list.files(pattern = "SmokingHistoryGenerator_.*.tar.gz")[1]
  
  # Run check
  result <- system(paste("R CMD check", tarball, "--as-cran --no-manual"))
  
  if (result != 0) {
    stop("R CMD check found issues. Please fix before committing.")
  }
}

cat("\n✓ R CMD check passed!\n")

