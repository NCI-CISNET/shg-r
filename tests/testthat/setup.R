# Sourced automatically by testthat before any test file.
# GitHub Actions sets CI=true; R CMD check does not, but GHA workflows do.

if (identical(tolower(Sys.getenv("CI", "")), "true") || identical(Sys.getenv("CI", ""), "1")) {
  message(
    "testthat (CI): R ", getRversion(), " | ", R.version$os,
    " | ", R.version$arch, " | endian=", .Platform$endian, " | wd=", getwd()
  )
}

# Set SHG_TEST_VERBOSE=1 in the workflow to print this block (e.g. options() for deeper diffs).
if (nzchar(Sys.getenv("SHG_TEST_VERBOSE", ""))) {
  message("testthat: SHG_TEST_VERBOSE is set — see tests/testthat.R for tips")
}
