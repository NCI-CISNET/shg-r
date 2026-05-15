test_that("RngStream fixed cohort: results and RNG fingerprint invariant to num_threads", {
  skip_on_cran()
  data_folder <- system.file("extdata", "2018", package = "SmokingHistoryGenerator")
  skip_if_not(nzchar(data_folder) && dir.exists(data_folder))

  n <- 100000L
  race <- 0L
  sex <- 0L
  cohort <- 1950L
  thread_opts <- c(1L, 4L, 8L, 12L)

  .strip_repro_audit <- function(rc) {
    drop <- c("timestamp", "results", "repro_digest")
    rc[!names(rc) %in% drop]
  }

  .run_with_threads <- function(reset_instance, repro_base) {
    ref_df <- NULL
    ref_fp <- NULL
    shg <- new(SHGInterface)
    shg$input_data_folder <- data_folder
    for (nt in thread_opts) {
      if (isTRUE(reset_instance)) {
        shg <- new(SHGInterface)
        shg$input_data_folder <- data_folder
      }
      cfg <- repro_base
      cfg$num_threads <- nt
      shg$useConfig(cfg)
      df <- shg$runSimFromFixedValues(n, race, sex, cohort, FALSE, NULL)
      fp <- shg$getReproConfig(debug = TRUE)$rng_state_fingerprint
      if (is.null(ref_df)) {
        ref_df <- df
        ref_fp <- fp
      } else {
        expect_equal(df, ref_df)
        expect_equal(fp, ref_fp)
      }
    }
  }

  shg0 <- new(SHGInterface)
  shg0$input_data_folder <- data_folder
  shg0$rng_strategy <- "RngStream"
  shg0$number_of_segments <- -1L
  shg0$num_threads <- -1L
  out <- shg0$runSimFromFixedValues(n, race, sex, cohort, TRUE, NULL)
  repro_base <- .strip_repro_audit(out$repro_config)

  .run_with_threads(TRUE, repro_base)
  .run_with_threads(FALSE, repro_base)
})
