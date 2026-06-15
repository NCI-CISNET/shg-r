#!/usr/bin/env Rscript
# Build split shg-params-style test zips from the legacy combined fixture.

args <- commandArgs(trailingOnly = TRUE)
pkg_root <- if (length(args) >= 1 && nzchar(args[[1]])) {
  normalizePath(args[[1]], mustWork = TRUE)
} else if (file.exists("DESCRIPTION")) {
  normalizePath(getwd())
} else {
  cmd <- commandArgs()
  f <- sub("^--file=", "", cmd[grep("^--file=", cmd)])
  if (!length(f)) stop("Run from package root or pass pkg_root as first argument.")
  normalizePath(file.path(dirname(f[[1]]), ".."), mustWork = TRUE)
}

combined <- file.path(pkg_root, "tests", "testdata", "usa-national@smok-2018-mort-2016.zip")
if (!file.exists(combined)) {
  stop("Combined fixture not found: ", combined)
}

out_dir <- file.path(pkg_root, "tests", "testdata")
smok_zip <- file.path(out_dir, "usa-national@smok-NHIS-2018.zip")
mort_zip <- file.path(out_dir, "usa-national@mort-v1.0.0.zip")

stage <- tempfile("shg_split_fixture_")
dir.create(stage, recursive = TRUE)
on.exit(unlink(stage, recursive = TRUE), add = TRUE)

utils::unzip(combined, exdir = stage)
roots <- list.dirs(stage, recursive = FALSE, full.names = TRUE)
roots <- roots[!grepl("__MACOSX", roots)]
root <- stage
if (length(roots) == 1 && dir.exists(file.path(roots[[1]], "smoking"))) {
  root <- roots[[1]]
}

smok_stage <- file.path(stage, "smok-staging")
mort_stage <- file.path(stage, "mort-staging")
dir.create(file.path(smok_stage, "params"), recursive = TRUE)
dir.create(file.path(mort_stage, "params"), recursive = TRUE)

for (f in c("initiation.csv", "cessation.csv", "cpd.csv")) {
  file.copy(file.path(root, "smoking", f), file.path(smok_stage, "params", f))
}
for (f in c("acm.csv", "ocm-excl-lung-cancer.csv")) {
  src <- file.path(root, "mortality", f)
  if (file.exists(src))
    file.copy(src, file.path(mort_stage, "params", f))
}

writeLines("domain: smoking\nrelease_key: usa-national@smok-NHIS-2018\n",
           file.path(smok_stage, "metadata.yml"))
writeLines("domain: mortality\nrelease_key: usa-national@mort-v1.0.0\nversion: 1.0.0\n",
           file.path(mort_stage, "metadata.yml"))

zip_dir <- function(zip_path, dir) {
  if (file.exists(zip_path)) unlink(zip_path)
  owd <- getwd()
  on.exit(setwd(owd), add = TRUE)
  setwd(dir)
  utils::zip(zip_path, files = list.files(".", recursive = TRUE, all.files = FALSE),
             flags = "-r9Xq")
}

zip_dir(smok_zip, smok_stage)
zip_dir(mort_zip, mort_stage)

message("Wrote:\n  ", smok_zip, "\n  ", mort_zip)
