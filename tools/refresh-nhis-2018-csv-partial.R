#!/usr/bin/env Rscript
# Rebuild NHIS-1965-2018 csv-partial (1940, 1950, 2010) from tests/testdata/NHIS-1965-2018/csv-complete/
# and sync into inst/extdata/2018/ and tests/testdata/NHIS-1965-2018/csv-partial/.
# Run from package root: Rscript tools/refresh-nhis-2018-csv-partial.R

argv <- commandArgs(trailingOnly = FALSE)
file_arg <- sub("^--file=", "", argv[grep("^--file=", argv, fixed = FALSE)[1L]])
repo_root <- if (nzchar(file_arg)) {
  normalizePath(file.path(dirname(file_arg), ".."), winslash = "/", mustWork = TRUE)
} else {
  normalizePath(getwd(), winslash = "/", mustWork = TRUE)
}
complete <- file.path(repo_root, "tests", "testdata", "NHIS-1965-2018", "csv-complete")
targets <- c(
  file.path(repo_root, "inst", "extdata", "2018"),
  file.path(repo_root, "tests", "testdata", "NHIS-1965-2018", "csv-partial")
)
cohorts_chr <- c("1940", "1950", "2010")
yobs_int <- as.integer(cohorts_chr)

stopifnot(dir.exists(complete))
for (t in targets) {
  stopifnot(dir.exists(t))
}

read_wide <- function(name) {
  path <- file.path(complete, name)
  d <- utils::read.csv(path, check.names = FALSE, stringsAsFactors = FALSE)
  miss <- setdiff(c("RACE", "SEX", "AGE", cohorts_chr), names(d))
  if (length(miss)) {
    stop("Missing columns in ", name, ": ", paste(miss, collapse = ", "))
  }
  d <- d[d$RACE == 0 & d$SEX == 0, , drop = FALSE]
  d <- d[order(d$AGE), , drop = FALSE]
  d[, c("RACE", "SEX", "AGE", cohorts_chr)]
}

write_wide <- function(df, rel_smoking) {
  for (t in targets) {
    out <- file.path(t, "smoking", basename(rel_smoking))
    dir.create(dirname(out), recursive = TRUE, showWarnings = FALSE)
    utils::write.csv(df, out, row.names = FALSE, quote = FALSE)
  }
}

write_mort <- function(df, fname) {
  for (t in targets) {
    out <- file.path(t, "mortality", fname)
    dir.create(dirname(out), recursive = TRUE, showWarnings = FALSE)
    utils::write.csv(df, out, row.names = FALSE, quote = FALSE)
  }
}

write_wide(read_wide("initiation.csv"), "initiation.csv")
write_wide(read_wide("cessation.csv"), "cessation.csv")

acm <- utils::read.csv(file.path(complete, "acm.csv"), check.names = FALSE, stringsAsFactors = FALSE)
acm <- acm[acm$RACE == 0 & acm$SEX == 0 & acm$YOB %in% yobs_int, , drop = FALSE]
acm <- acm[order(acm$YOB, acm$AGE), , drop = FALSE]
write_mort(acm, "acm.csv")

ocm <- utils::read.csv(file.path(complete, "ocm-excl-lung-cancer.csv"), check.names = FALSE, stringsAsFactors = FALSE)
ocm <- ocm[ocm$RACE == 0 & ocm$SEX == 0 & ocm$YOB %in% yobs_int, , drop = FALSE]
ocm <- ocm[order(ocm$YOB, ocm$AGE), , drop = FALSE]
write_mort(ocm, "ocm-excl-lung-cancer.csv")

cpd <- utils::read.csv(file.path(complete, "cpd.csv"), check.names = FALSE, stringsAsFactors = FALSE)
cpd <- cpd[
  cpd$RACE == 0 & cpd$SEX == 0 &
    cpd$START_YOB %in% yobs_int & cpd$START_YOB == cpd$END_YOB,
  ,
  drop = FALSE
]
cpd$.yord <- match(cpd$START_YOB, yobs_int)
cpd <- cpd[order(cpd$AGE, cpd$.yord), , drop = FALSE]
cpd$.yord <- NULL
write_wide(cpd, basename(file.path(complete, "cpd.csv")))

message("Wrote 1940/1950/2010 partials to:\n  ", paste(targets, collapse = "\n  "))
