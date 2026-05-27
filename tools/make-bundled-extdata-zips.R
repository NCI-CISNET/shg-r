#!/usr/bin/env Rscript
# Build shg-params-style smoking/mortality zips from inst/extdata/2018/{smoking,mortality}.
# Installed path: system.file("extdata", "2018", "bundled-smok.zip", package = "SmokingHistoryGenerator")

args <- commandArgs(trailingOnly = TRUE)
pkg_root <- if (length(args) >= 1 && nzchar(args[[1]])) {
  normalizePath(args[[1]], mustWork = TRUE)
} else if (file.exists("DESCRIPTION")) {
  normalizePath(getwd(), winslash = "/")
} else {
  stop("Run from the shg-r package root or pass pkg_root as the first argument.")
}

base <- file.path(pkg_root, "inst", "extdata", "2018")
smok_csv <- file.path(base, "smoking")
mort_csv <- file.path(base, "mortality")
for (label in c(smok_csv, mort_csv)) {
  if (!dir.exists(label)) {
    stop("Missing directory: ", label)
  }
}

zip_one <- function(zip_path, stage_root, params_files, meta_lines) {
  if (file.exists(zip_path)) {
    unlink(zip_path)
  }
  params_dir <- file.path(stage_root, "params")
  dir.create(params_dir, recursive = TRUE, showWarnings = FALSE)
  for (spec in params_files) {
    file.copy(spec$from, file.path(params_dir, spec$to), overwrite = TRUE)
  }
  writeLines(meta_lines, file.path(stage_root, "metadata.yml"))
  owd <- getwd()
  on.exit(setwd(owd), add = TRUE)
  setwd(stage_root)
  utils::zip(zip_path, files = list.files(".", recursive = TRUE, all.files = FALSE),
             flags = "-r9Xq")
}

stage <- tempfile("shg_bundled_zip_")
dir.create(stage, recursive = TRUE)
on.exit(unlink(stage, recursive = TRUE), add = TRUE)

smok_zip <- file.path(base, "bundled-smok.zip")
smok_stage <- file.path(stage, "smok")
dir.create(smok_stage, showWarnings = FALSE)
zip_one(
  smok_zip,
  smok_stage,
  list(
    list(from = file.path(smok_csv, "initiation.csv"), to = "initiation.csv"),
    list(from = file.path(smok_csv, "cessation.csv"), to = "cessation.csv"),
    list(from = file.path(smok_csv, "cpd.csv"), to = "cpd.csv")
  ),
  c(
    "domain: smoking",
    "release_key: bundled@smok-NHIS-2018-partial",
    "note: CRAN-sized NHIS 2018 csv-partial cohorts (see inst/extdata/2018/smoking/)"
  )
)

mort_zip <- file.path(base, "bundled-mort.zip")
mort_stage <- file.path(stage, "mort")
dir.create(mort_stage, showWarnings = FALSE)
zip_one(
  mort_zip,
  mort_stage,
  list(
    list(from = file.path(mort_csv, "acm.csv"), to = "acm.csv"),
    list(from = file.path(mort_csv, "ocm-excl-lung-cancer.csv"), to = "ocm-excl-lung-cancer.csv")
  ),
  c(
    "domain: mortality",
    "release_key: bundled@mort-v1.0.0-partial",
    "version: 1.0.0",
    "note: CRAN-sized mortality tables (see inst/extdata/2018/mortality/)"
  )
)

message("Wrote:\n  ", smok_zip, "\n  ", mort_zip)
