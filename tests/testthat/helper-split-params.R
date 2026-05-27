shg_test_split_param_zips <- function() {
  smok <- testthat::test_path("../testdata/usa-national@smok-NHIS-2018.zip")
  mort <- testthat::test_path("../testdata/usa-national@mort-v1.0.0.zip")
  testthat::skip_if_not(file.exists(smok) && file.exists(mort))
  list(smok = smok, mort = mort)
}

shg_test_with_param_cache <- function(code) {
  tmp_cache <- tempfile("shg_param_cache_")
  dir.create(tmp_cache)
  old <- Sys.getenv("R_USER_CACHE_DIR", "")
  Sys.setenv(R_USER_CACHE_DIR = tmp_cache)
  on.exit({
    if (nzchar(old)) Sys.setenv(R_USER_CACHE_DIR = old)
    else Sys.unsetenv("R_USER_CACHE_DIR")
    unlink(tmp_cache, recursive = TRUE)
  }, add = TRUE)
  force(code)
}
