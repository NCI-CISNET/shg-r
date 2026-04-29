# Updating SmokingHistoryGenerator

When a new version is released, follow these steps to update your installation.

## Quick Update

```r
# Step 1: Uninstall current package
remove.packages("SmokingHistoryGenerator")

# Step 2: Restart R/RStudio (important!)

# Step 3: Reinstall from GitHub (installs from master branch by default)
devtools::install_github("NCI-CISNET/shg-r")
# Or install from a specific branch: devtools::install_github("NCI-CISNET/shg-r@branch-name")
# Or install from a specific tag: devtools::install_github("NCI-CISNET/shg-r@v1.0.0")
```

## Complete Update Script

Copy and paste this into R:

```r
# Uninstall existing package
tryCatch({
  remove.packages("SmokingHistoryGenerator")
  cat("Package removed successfully.\n")
}, error = function(e) {
  cat("Package may not have been installed, or already removed.\n")
})

# Restart R/RStudio before continuing!

# Install from GitHub (defaults to master branch)
devtools::install_github("NCI-CISNET/shg-r")
# OR install from a specific branch
devtools::install_github("NCI-CISNET/shg-r@branch-name")
# OR install from a specific tag/release
devtools::install_github("NCI-CISNET/shg-r@v1.0.0")

# Load and verify
library(SmokingHistoryGenerator)
cat("Package updated successfully!\n")
```

## Important Notes

- **Always restart R/RStudio** after uninstalling and before reinstalling
- The package compiles C++ code, so updates may take a few minutes
- If you encounter errors, make sure you have the latest build tools installed

