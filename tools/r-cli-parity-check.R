#!/usr/bin/env Rscript
# Parity: portable YAML workflow (save_config -> load_config + shg_run) vs shg-cli
# legacy text configs built from the same post-load engine state.
#
# R path (per scenario):
#   1) load_params -> tune engine -> runSimFromFixedValues -> shg_save_config(run.yml)
#   2) Fresh shg: config <- shg_load_config(shg, run.yml); sim <- shg_run(shg, config)
#
# CLI path: after shg_load_config, write input.txt with engine lines from getConfig() and
# INIT/CESS/MORT/CPD paths under <out_dir>/_bundle_extract/ (zip unzipped once there so
# the CLI always reads stable files alongside the report).
#
# Usage (shell, from shg-r repo root — not the R console):
#   SHG_CLI=/path/to/shg-cli/lbc_smokehist.exe Rscript tools/r-cli-parity-check.R
# Optional: SHG_PARAMS_ZIP, SHG_PARITY_N=100000, --out-dir=, --report=, --verbose
# Legacy RunWebVersion text (older shg-cli parsers):
#   --legacy-input   or   SHG_CLI_LEGACY_INPUT=1
# Adds OCD_PROB= (and keeps MORTALITY_PROB=), CESSATION_YEAR=, and flattens bundled
# CSV paths under the extract root (initiation.csv, …). For RngStream, extra SEED_INIT/
# SEED_CESS/SEED_OCD/SEED_MISC lines are omitted by default (they confuse newer CLIs);
# set SHG_CLI_LEGACY_RS_MT_SEEDS=1 to emit them for antique builds that error without.
# Stacked long CSVs (`parity_all_*_core_long.csv`) are large; skip with SHG_CLI_PARITY_NO_STACK=1.
#
# Legacy wide tables (NHIS-style `.txt` under one folder) for both R and CLI:
#   SHG_LEGACY_TXT_ROOT=/path/to/dir
# Optional overrides (relative to that directory, no `..`, not absolute):
#   SHG_LEGACY_TXT_INITIATION, SHG_LEGACY_TXT_CESSATION, SHG_LEGACY_TXT_CPD,
#   SHG_LEGACY_TXT_ACM, SHG_LEGACY_TXT_OCM
# Defaults: initiation.txt, cessation.txt, cpd.txt, acm.txt, ocm-excl-lung-cancer.txt
# (see tests/testdata/2018/legacy-complete/). Absolute `SHG_LEGACY_TXT_ROOT` is kept
# as given (not `normalizePath`'d first) so a symlink with a space-free path can target a
# folder whose name contains spaces. Table lines in `input.txt` avoid expanding that symlink
# when `file.path(root, name)` already exists; paths with spaces or quotes are double-quoted.
# The portable YAML round-trip is skipped (it would reload CSV from the zip). You still need
# SHG_PARAMS_ZIP so
# `shg_load_params()` can resolve the bundle; table paths are then overridden to these files.
#
# Requires tests/testdata/usa-national@smok-2018-mort-2016.zip (or SHG_PARAMS_ZIP) for full
# cohort/sex coverage; uses one R_USER_CACHE_DIR under out_dir for all scenarios.

suppressPackageStartupMessages({
  if (!requireNamespace("pkgload", quietly = TRUE)) {
    stop("Install pkgload (e.g. devtools) to load the package from source.", call. = FALSE)
  }
})

parse_args <- function() {
  a <- commandArgs(trailingOnly = TRUE)
  out <- list(
    out_dir = "tmp/shg-r-cli-parity",
    report = "tmp/shg-r-cli-parity-report.md",
    verbose = FALSE,
    legacy_input = FALSE
  )
  for (x in a) {
    if (grepl("^--out-dir=", x)) out$out_dir <- sub("^--out-dir=", "", x)
    if (grepl("^--report=", x)) out$report <- sub("^--report=", "", x)
    if (x == "--verbose") out$verbose <- TRUE
    if (x == "--legacy-input") out$legacy_input <- TRUE
  }
  out
}

args <- parse_args()
legacy_env <- tolower(trimws(Sys.getenv("SHG_CLI_LEGACY_INPUT", "")))
args$legacy_input <- isTRUE(args$legacy_input) ||
  legacy_env %in% c("1", "true", "yes", "y")
rs_mt_env <- tolower(trimws(Sys.getenv("SHG_CLI_LEGACY_RS_MT_SEEDS", "")))
args$legacy_rs_mt_seeds <- rs_mt_env %in% c("1", "true", "yes", "y")
ca <- commandArgs(trailingOnly = FALSE)
ff <- sub("^--file=", "", ca[grepl("^--file=", ca)][1L])
if (nzchar(ff) && file.exists(ff)) {
  repo_root <- normalizePath(file.path(dirname(normalizePath(ff)), ".."))
} else {
  repo_root <- normalizePath(getwd())
}
if (!file.exists(file.path(repo_root, "DESCRIPTION"))) {
  stop(
    "Could not locate package root (DESCRIPTION). Run from shg-r root or via ",
    "Rscript --file=tools/r-cli-parity-check.R",
    call. = FALSE
  )
}

cli_default <- normalizePath(
  file.path(dirname(repo_root), "shg-cli", "lbc_smokehist.exe"),
  mustWork = FALSE
)
cli_bin <- Sys.getenv("SHG_CLI", if (file.exists(cli_default)) cli_default else "")
if (!nzchar(cli_bin) || !file.exists(cli_bin)) {
  stop(
    "Set SHG_CLI to the shg-cli executable (e.g. export SHG_CLI=../shg-cli/lbc_smokehist.exe).",
    call. = FALSE
  )
}

zip_default <- file.path(repo_root, "tests", "testdata", "usa-national@smok-2018-mort-2016.zip")
zip_path <- Sys.getenv("SHG_PARAMS_ZIP", zip_default)
zip_path <- normalizePath(zip_path, winslash = "/", mustWork = FALSE)
if (!file.exists(zip_path)) {
  stop(
    "Parameter bundle not found: ", zip_path,
    "\nSet SHG_PARAMS_ZIP to usa-national@smok-2018-mort-2016.zip or another compatible zip.",
    call. = FALSE
  )
}

legacy_txt_env <- trimws(Sys.getenv("SHG_LEGACY_TXT_ROOT", ""))
legacy_txt_mode <- nzchar(legacy_txt_env)
legacy_txt_root_iface <- ""
legacy_txt_root_resolved <- ""
if (isTRUE(legacy_txt_mode)) {
  absish <- grepl("^/", legacy_txt_env) || grepl("^[A-Za-z]:[/\\\\]", legacy_txt_env)
  legacy_txt_root_iface <- if (isTRUE(absish)) {
    # Do not normalizePath: preserves symlinks and avoids resolving to paths some CLIs
    # mishandle (e.g. directory names containing spaces).
    legacy_txt_env
  } else {
    normalizePath(file.path(repo_root, legacy_txt_env), winslash = "/", mustWork = FALSE)
  }
  legacy_txt_root_resolved <- normalizePath(legacy_txt_root_iface, winslash = "/", mustWork = TRUE)
  if (!dir.exists(legacy_txt_root_resolved)) {
    stop("SHG_LEGACY_TXT_ROOT is not a directory: ", legacy_txt_root_iface, call. = FALSE)
  }
}
legacy_txt_root <- legacy_txt_root_iface

out_dir <- if (grepl("^/", args$out_dir) || grepl("^[A-Za-z]:[/\\\\]", args$out_dir)) {
  args$out_dir
} else {
  file.path(repo_root, args$out_dir)
}
out_dir <- normalizePath(out_dir, winslash = "/", mustWork = FALSE)
dir.create(out_dir, recursive = TRUE, showWarnings = FALSE)

report_path <- if (grepl("^/", args$report) || grepl("^[A-Za-z]:[/\\\\]", args$report)) {
  args$report
} else {
  file.path(repo_root, args$report)
}
dir.create(dirname(report_path), recursive = TRUE, showWarnings = FALSE)

pkgload::load_all(repo_root, quiet = TRUE)

parity_n <- as.integer(Sys.getenv("SHG_PARITY_N", "100000"))
if (is.na(parity_n) || parity_n < 1L) {
  stop("SHG_PARITY_N must be a positive integer.", call. = FALSE)
}
rs_seed_a <- c(12345, 12345, 12345, 12345, 12345, 12345)
rs_seed_b <- c(777, 888, 999, 1111, 2222, 3333)
mt_seed_a <- c(1898587603L, 1468371936L, 1551308340L, 1590227640L)
mt_seed_b <- c(42L, 99L, 100L, 200L)

fmt_rs_seed <- function(v) paste(as.integer(v), collapse = ",")
fmt_mt_seed <- function(v) paste(as.integer(v), collapse = ",")

# Scenario definitions applied BEFORE the fixed cohort run that produces portable YAML.
# Each list element must include: id, params_cell (HTML for markdown table), mortality,
# rng_strategy, num_threads, number_of_segments, immediate_cessation_year, race, sex,
# cohort_year, cpd_format, plus rngstream_seed OR mt_seeds.
# Optional: repeat_n = <int> overrides SHG_PARITY_N for that scenario only (saved in YAML).
build_parity_scenarios <- function() {
  grid_cohorts <- c(1920L, 1940L, 1960L, 1980L, 2000L)
  out <- list()
  rs_pairs <- list(
    list(tag = "seedA", v = rs_seed_a, lab = paste0("RS ", fmt_rs_seed(rs_seed_a))),
    list(tag = "seedB", v = rs_seed_b, lab = paste0("RS ", fmt_rs_seed(rs_seed_b)))
  )
  mt_pairs <- list(
    list(tag = "seedA", v = mt_seed_a, lab = paste0("MT ", fmt_mt_seed(mt_seed_a))),
    list(tag = "seedB", v = mt_seed_b, lab = paste0("MT ", fmt_mt_seed(mt_seed_b)))
  )
  for (y in grid_cohorts) {
    for (pair in rs_pairs) {
      out <- c(out, list(list(
        id = sprintf("grid_rs_y%d_%s", y, pair$tag),
        params_cell = paste0(
          "<b>RngStream</b><br>yob=", y, " race=0 sex=M acm<br>",
          "threads=1 segments=1<br>", pair$lab
        ),
        mortality = "acm",
        rng_strategy = "RngStream",
        rngstream_seed = as.numeric(pair$v),
        num_threads = 1L,
        number_of_segments = 1L,
        immediate_cessation_year = 0L,
        race = 0L,
        sex = 0L,
        cohort_year = y,
        cpd_format = "none"
      )))
    }
  }
  for (y in grid_cohorts) {
    for (pair in mt_pairs) {
      out <- c(out, list(list(
        id = sprintf("grid_mt_y%d_%s", y, pair$tag),
        params_cell = paste0(
          "<b>MersenneTwister</b><br>yob=", y, " race=0 sex=M acm<br>",
          "threads=1 segments=1<br>", pair$lab
        ),
        mortality = "acm",
        rng_strategy = "MersenneTwister",
        mt_seeds = pair$v,
        num_threads = 1L,
        number_of_segments = 1L,
        immediate_cessation_year = 0L,
        race = 0L,
        sex = 0L,
        cohort_year = y,
        cpd_format = "none"
      )))
    }
  }
  out <- c(out, list(
    list(
      id = "par_rs_y2010_f_ocm_seedB",
      params_cell = paste0(
        "<b>RngStream</b><br>yob=2010 race=0 sex=F ocm<br>",
        "threads=1 segments=1<br>RS ", fmt_rs_seed(rs_seed_b)
      ),
      mortality = "ocm",
      rng_strategy = "RngStream",
      rngstream_seed = rs_seed_b,
      num_threads = 1L,
      number_of_segments = 1L,
      immediate_cessation_year = 0L,
      race = 0L,
      sex = 1L,
      cohort_year = 2010L,
      cpd_format = "none"
    ),
    list(
      id = "par_mt_y2010_f_ocm_seedA",
      params_cell = paste0(
        "<b>MersenneTwister</b><br>yob=2010 race=0 sex=F ocm<br>",
        "threads=1 segments=1<br>MT ", fmt_mt_seed(mt_seed_a)
      ),
      mortality = "ocm",
      rng_strategy = "MersenneTwister",
      mt_seeds = mt_seed_a,
      num_threads = 1L,
      number_of_segments = 1L,
      immediate_cessation_year = 0L,
      race = 0L,
      sex = 1L,
      cohort_year = 2010L,
      cpd_format = "none"
    ),
    list(
      id = "stress_rs_auto_2010_f",
      params_cell = paste0(
        "<b>RngStream</b><br>yob=2010 race=0 sex=F ocm<br>",
        "threads=-1 seg=-1 (auto)<br>RS ", fmt_rs_seed(rs_seed_a)
      ),
      mortality = "ocm",
      rng_strategy = "RngStream",
      rngstream_seed = rs_seed_a,
      num_threads = -1L,
      number_of_segments = -1L,
      immediate_cessation_year = 0L,
      race = 0L,
      sex = 1L,
      cohort_year = 2010L,
      cpd_format = "none"
    ),
    list(
      id = "stress_rs_manual_1980_m",
      params_cell = paste0(
        "<b>RngStream</b><br>yob=1980 race=0 sex=M acm<br>",
        "threads=2 segments=8<br>RS ", fmt_rs_seed(rs_seed_a)
      ),
      mortality = "acm",
      rng_strategy = "RngStream",
      rngstream_seed = rs_seed_a,
      num_threads = 2L,
      number_of_segments = 8L,
      immediate_cessation_year = 0L,
      race = 0L,
      sex = 0L,
      cohort_year = 1980L,
      cpd_format = "none"
    ),
    list(
      id = "stress_rs_remainder_206_s8",
      params_cell = paste0(
        "<b>RngStream</b><br>repeat=206 (206 mod 8 != 0) yob=1980 M acm<br>",
        "threads=1 segments=8<br>RS ", fmt_rs_seed(rs_seed_a)
      ),
      mortality = "acm",
      rng_strategy = "RngStream",
      rngstream_seed = rs_seed_a,
      num_threads = 1L,
      number_of_segments = 8L,
      immediate_cessation_year = 0L,
      race = 0L,
      sex = 0L,
      cohort_year = 1980L,
      cpd_format = "none",
      repeat_n = 206L
    ),
    list(
      id = "stress_rs_remainder_503_s7",
      params_cell = paste0(
        "<b>RngStream</b><br>repeat=503 (503 mod 7 != 0) yob=1980 M acm<br>",
        "threads=1 segments=7<br>RS ", fmt_rs_seed(rs_seed_a)
      ),
      mortality = "acm",
      rng_strategy = "RngStream",
      rngstream_seed = rs_seed_a,
      num_threads = 1L,
      number_of_segments = 7L,
      immediate_cessation_year = 0L,
      race = 0L,
      sex = 0L,
      cohort_year = 1980L,
      cpd_format = "none",
      repeat_n = 503L
    ),
    list(
      id = "stress_mt_2005_f_immediate",
      params_cell = paste0(
        "<b>MersenneTwister</b><br>yob=2005 race=0 sex=F ocm<br>",
        "threads=1 seg=1 immediate cess 2050<br>MT ", fmt_mt_seed(mt_seed_a)
      ),
      mortality = "ocm",
      rng_strategy = "MersenneTwister",
      mt_seeds = mt_seed_a,
      num_threads = 1L,
      number_of_segments = 1L,
      immediate_cessation_year = 2050L,
      race = 0L,
      sex = 1L,
      cohort_year = 2005L,
      cpd_format = "none"
    )
  ))
  out
}

scenarios <- build_parity_scenarios()

shg_class <- getFromNamespace("SHGInterface", "SmokingHistoryGenerator")

# Match .shg_snapshot_root() in R/load_params.R (zip may add one top-level folder).
snapshot_root_for_bundle <- function(exdir_parent) {
  top <- list.files(exdir_parent, full.names = TRUE)
  top <- top[!grepl("__MACOSX", top)]
  dirs <- top[file.info(top)$isdir]
  if (length(dirs) == 1L) {
    cand <- dirs[[1L]]
    if (dir.exists(file.path(cand, "smoking")) || dir.exists(file.path(cand, "mortality"))) {
      return(cand)
    }
  }
  exdir_parent
}

#' Unzip parameter bundle once under out_dir/_bundle_extract; return snapshot root
#' (directory that contains smok/ and mort/).
parity_bundle_cli_root <- function(zip_abs, out_dir) {
  ex <- file.path(normalizePath(out_dir, winslash = "/", mustWork = FALSE), "_bundle_extract")
  dir.create(ex, recursive = TRUE, showWarnings = FALSE)
  marker <- file.path(snapshot_root_for_bundle(ex), "smoking", "initiation.csv")
  if (!file.exists(marker)) {
    utils::unzip(zip_abs, exdir = ex)
  }
  normalizePath(snapshot_root_for_bundle(ex), winslash = "/", mustWork = TRUE)
}

abs_input_path <- function(root, rel) {
  p <- file.path(root, rel)
  normalizePath(p, winslash = "/", mustWork = TRUE)
}

#' Absolute path to a table under \code{root}. If \code{file.path(root, rel)} exists, return it
#' without \code{normalizePath()} so symlinks in \code{root} are not expanded (some CLIs
#' mishandle the resolved path).
safe_abs_table <- function(root, rel) {
  p <- file.path(root, rel)
  if (file.exists(p)) {
    return(p)
  }
  normalizePath(p, winslash = "/", mustWork = TRUE)
}

#' Quote a filesystem path for shg-cli legacy \code{input.txt} when it contains whitespace
#' (otherwise parsers split on spaces and paths like \verb{SHG6.3.4 6/} break).
legacy_cli_path_token <- function(p) {
  p <- as.character(p)[1L]
  if (!nzchar(p)) {
    return(p)
  }
  if (grepl("[[:space:]\"]", p, perl = TRUE)) {
    paste0('"', gsub("\"", "\\\\\"", p, fixed = TRUE), '"')
  } else {
    p
  }
}

#' Default basenames when \code{SHG_LEGACY_TXT_*} env vars are unset.
legacy_txt_default_basenames <- function() {
  list(
    initiation = "initiation.txt",
    cessation = "cessation.txt",
    cpd = "cpd.txt",
    acm = "acm.txt",
    ocm = "ocm-excl-lung-cancer.txt"
  )
}

#' Read optional \code{SHG_LEGACY_TXT_INITIATION}, …, \code{_OCM}; validate files under \code{root}.
legacy_txt_read_specs <- function(root) {
  root <- normalizePath(root, winslash = "/", mustWork = TRUE)
  def <- legacy_txt_default_basenames()
  getv <- function(key, envnm) {
    v <- trimws(Sys.getenv(envnm, ""))
    if (!nzchar(v)) {
      return(def[[key]])
    }
    if (grepl("..", v, fixed = TRUE)) {
      stop(envnm, " must not contain '..'.", call. = FALSE)
    }
    if (startsWith(v, "/") || grepl("^[A-Za-z]:[/\\\\]", v)) {
      stop(
        envnm, " must be a relative path under SHG_LEGACY_TXT_ROOT, not an absolute path: ",
        v,
        call. = FALSE
      )
    }
    v
  }
  out <- list(
    initiation = getv("initiation", "SHG_LEGACY_TXT_INITIATION"),
    cessation = getv("cessation", "SHG_LEGACY_TXT_CESSATION"),
    cpd = getv("cpd", "SHG_LEGACY_TXT_CPD"),
    acm = getv("acm", "SHG_LEGACY_TXT_ACM"),
    ocm = getv("ocm", "SHG_LEGACY_TXT_OCM")
  )
  miss <- character(0)
  for (nm in names(out)) {
    full <- normalizePath(file.path(root, out[[nm]]), winslash = "/", mustWork = FALSE)
    if (!file.exists(full)) {
      miss <- c(miss, paste0("  ", nm, ": ", full))
    }
  }
  if (length(miss)) {
    stop(
      "Legacy wide tables not found under SHG_LEGACY_TXT_ROOT=\n  ", root,
      "\nMissing:\n", paste(miss, collapse = "\n"),
      "\nSet SHG_LEGACY_TXT_INITIATION / _CESSATION / _CPD / _ACM / _OCM if basenames differ.",
      call. = FALSE
    )
  }
  out
}

#' Copy smok/mort tables to flat names under \code{cli_root} for older CLIs
#' that only accept INIT_PROB=.../initiation.csv style paths.
#' Point the interface at flat legacy `.txt` tables under \code{root} (NHIS wide format).
legacy_apply_txt_table_paths <- function(shg, root, mortality, specs) {
  root <- normalizePath(root, winslash = "/", mustWork = TRUE)
  mort <- match.arg(mortality, c("acm", "ocm"))
  shg$input_data_folder <- root
  shg$initiation_filename <- specs$initiation
  shg$cessation_filename <- specs$cessation
  shg$cpd_filename <- specs$cpd
  shg$mortality_filename <- if (identical(mort, "acm")) specs$acm else specs$ocm
  invisible(shg)
}

legacy_txt_specs <- if (isTRUE(legacy_txt_mode)) {
  legacy_txt_read_specs(legacy_txt_root_resolved)
} else {
  NULL
}

legacy_flatten_bundle_tables <- function(cli_root) {
  root <- normalizePath(cli_root, winslash = "/", mustWork = TRUE)
  pairs <- list(
    c("smok/initiation.csv", "initiation.csv"),
    c("smok/cessation.csv", "cessation.csv"),
    c("smok/cpd.csv", "cpd.csv"),
    c("mort/acm.csv", "acm.csv"),
    c("mort/ocm-excl-lung-cancer.csv", "ocm-excl-lung-cancer.csv")
  )
  for (pr in pairs) {
    src <- file.path(root, pr[1L])
    dst <- file.path(root, pr[2L])
    if (file.exists(src)) {
      file.copy(src, dst, overwrite = TRUE)
    }
  }
  invisible(root)
}

#' Build legacy RunWebVersion text from an SHGInterface after shg_load_config (paths + engine).
#' If \code{cli_param_root} is set, INIT/CESS/MORT/CPD lines use that folder (stable extract for
#' the CLI); otherwise \code{shg$input_data_folder} is used.
#'
#' When \code{legacy_input} is \code{TRUE}, emit keys expected by older shg-cli builds
#' (OCD_PROB= as well as MORTALITY_PROB=; CESSATION_YEAR=; flat INIT/CESS/CPD paths).
#' Optional \code{legacy_rs_mt_seeds}: for RngStream, also emit SEED_INIT/…/SEED_MISC
#' (only for very old CLIs; breaks parity vs R on newer engines if enabled).
legacy_txt_from_loaded_shg <- function(
    shg,
    bundle,
    output_txt,
    output_data,
    error_txt,
    cli_param_root = NULL,
    legacy_input = FALSE,
    legacy_rs_mt_seeds = FALSE) {
  root <- if (length(cli_param_root) == 1L && nzchar(cli_param_root[1L])) {
    cli_param_root[1L]
  } else {
    shg$input_data_folder
  }
  gc <- shg$getConfig(debug = FALSE)
  rng <- gc$rng_strategy
  seeds <- unlist(gc$seeds, use.names = FALSE)
  seeds <- as.numeric(seeds)
  nseed <- length(seeds)
  if (nseed < 4L) {
    stop("getConfig()$seeds must have at least 4 values for legacy CLI input.", call. = FALSE)
  }
  four <- as.integer(seeds[1:4])

  rep <- as.integer(bundle[["repeat"]])[1L]
  race <- as.integer(bundle[["race"]])[1L]
  sex <- as.integer(bundle[["sex"]])[1L]
  yob <- as.integer(bundle[["cohort_year"]])[1L]
  ic <- if (is.null(bundle$immediate_cessation_year)) {
    0L
  } else {
    as.integer(bundle$immediate_cessation_year)[1L]
  }
  nt <- as.integer(gc$num_threads)[1L]
  ns <- as.integer(gc$number_of_segments)[1L]

  if (isTRUE(legacy_input)) {
    # Flat names under cli_param_root (flattened CSV or legacy .txt bundle).
    init <- safe_abs_table(root, basename(shg$initiation_filename))
    cess <- safe_abs_table(root, basename(shg$cessation_filename))
    cpd <- safe_abs_table(root, basename(shg$cpd_filename))
    mort <- safe_abs_table(root, basename(shg$mortality_filename))
  } else {
    init <- safe_abs_table(root, shg$initiation_filename)
    cess <- safe_abs_table(root, shg$cessation_filename)
    mort <- safe_abs_table(root, shg$mortality_filename)
    cpd <- safe_abs_table(root, shg$cpd_filename)
  }

  out_data <- normalizePath(output_data, winslash = "/", mustWork = FALSE)
  out_err <- normalizePath(error_txt, winslash = "/", mustWork = FALSE)
  init_t <- legacy_cli_path_token(init)
  cess_t <- legacy_cli_path_token(cess)
  mort_t <- legacy_cli_path_token(mort)
  cpd_t <- legacy_cli_path_token(cpd)
  out_data_t <- legacy_cli_path_token(out_data)
  out_err_t <- legacy_cli_path_token(out_err)

  seed_quad <- c(
    paste0("SEED_INIT=", four[1]),
    paste0("SEED_CESS=", four[2]),
    if (isTRUE(legacy_input)) {
      paste0("SEED_OCD=", four[3])
    } else {
      paste0("SEED_MORTALITY=", four[3])
    },
    paste0("SEED_MISC=", four[4])
  )

  rng_block <- if (isTRUE(legacy_input)) {
    if (identical(rng, "RngStream")) {
      rs_head <- c(paste0("RNGSTRATEGY=", rng))
      if (isTRUE(legacy_rs_mt_seeds)) {
        rs_head <- c(rs_head, seed_quad)
      }
      c(
        rs_head,
        paste0("RNGSTREAM_SEED=", paste(as.integer(seeds), collapse = ",")),
        paste0("NUM_THREADS=", nt),
        paste0("NUM_SEGMENTS=", ns)
      )
    } else {
      c(
        paste0("RNGSTRATEGY=", rng),
        seed_quad,
        paste0("NUM_THREADS=", nt),
        paste0("NUM_SEGMENTS=", ns)
      )
    }
  } else if (identical(rng, "RngStream")) {
    c(
      paste0("RNGSTRATEGY=", rng),
      paste0("RNGSTREAM_SEED=", paste(as.integer(seeds), collapse = ",")),
      paste0("NUM_THREADS=", nt),
      paste0("NUM_SEGMENTS=", ns)
    )
  } else {
    c(
      paste0("RNGSTRATEGY=", rng),
      seed_quad,
      paste0("NUM_THREADS=", nt),
      paste0("NUM_SEGMENTS=", ns)
    )
  }

  demo_lines <- c(
    "",
    paste0("RACE=", race),
    paste0("SEX=", sex),
    paste0("YOB=", yob),
    paste0("CESSATION_YR=", ic),
    if (isTRUE(legacy_input)) paste0("CESSATION_YEAR=", ic) else NULL,
    paste0("REPEAT=", rep),
    ""
  )

  table_lines <- if (isTRUE(legacy_input)) {
    c(
      paste0("INIT_PROB=", init_t),
      paste0("CESS_PROB=", cess_t),
      paste0("OCD_PROB=", mort_t),
      paste0("MORTALITY_PROB=", mort_t),
      paste0("CPD_DATA=", cpd_t),
      ""
    )
  } else {
    c(
      paste0("INIT_PROB=", init_t),
      paste0("CESS_PROB=", cess_t),
      paste0("MORTALITY_PROB=", mort_t),
      paste0("CPD_DATA=", cpd_t),
      ""
    )
  }

  lines <- c(
    rng_block,
    demo_lines,
    table_lines,
    paste0("OUTPUTFILE=", out_data_t),
    paste0("ERRORFILE=", out_err_t)
  )
  writeLines(lines, output_txt, useBytes = TRUE)
  invisible(output_txt)
}

read_cli_data <- function(output_path) {
  raw <- readLines(output_path, warn = FALSE, encoding = "UTF-8")
  raw <- gsub("\r", "", raw, fixed = TRUE)
  trimmed <- trimws(raw)
  i0 <- which(trimmed == "<RUN>")
  i1 <- which(trimmed == "</RUN>")
  if (!length(i0) || !length(i1)) {
    stop("No <RUN> block in: ", output_path, call. = FALSE)
  }
  body <- raw[(i0[1L] + 1L):(i1[1L] - 1L)]
  body <- body[nzchar(trimws(body))]
  is_row <- grepl("^[0-9-]", trimws(body)) & grepl(";", body, fixed = TRUE)
  body <- body[is_row]
  parse_one <- function(ln) {
    p <- strsplit(ln, ";", fixed = TRUE)[[1L]]
    if (length(p) < 6L) {
      return(NULL)
    }
    v <- suppressWarnings(as.integer(p[1:6]))
    if (anyNA(v)) {
      return(NULL)
    }
    v
  }
  rows <- lapply(body, parse_one)
  ok <- !vapply(rows, is.null, logical(1L))
  rows <- rows[ok]
  if (!length(rows)) {
    stop("No data rows parsed in: ", output_path, call. = FALSE)
  }
  mat <- do.call(rbind, rows)
  colnames(mat) <- c(
    "race", "sex", "birth_cohort",
    "smoking_initiation_age", "smoking_cessation_age", "age_at_death"
  )
  as.data.frame(mat, stringsAsFactors = FALSE)
}

run_cli <- function(input_txt) {
  err_file <- tempfile("cli_stderr_", fileext = ".txt")
  on.exit(unlink(err_file), add = TRUE)
  t0 <- proc.time()[["elapsed"]]
  rc <- system2(cli_bin, input_txt, stdout = FALSE, stderr = err_file)
  t1 <- proc.time()[["elapsed"]]
  err_txt <- if (file.exists(err_file)) {
    paste(readLines(err_file, warn = FALSE), collapse = "\n")
  } else {
    ""
  }
  list(rc = rc, stderr = err_txt, elapsed_sec = t1 - t0)
}

parity_core_cols <- c("smoking_initiation_age", "smoking_cessation_age", "age_at_death")

compare_core <- function(df_r, df_cli) {
  cols <- parity_core_cols
  if (!all(cols %in% names(df_r))) {
    return(list(ok = FALSE, reason = "R result missing core columns"))
  }
  if (!all(cols %in% names(df_cli))) {
    return(list(ok = FALSE, reason = "CLI parse missing core columns"))
  }
  if (nrow(df_r) != nrow(df_cli)) {
    return(list(
      ok = FALSE,
      reason = sprintf("row count mismatch R=%d CLI=%d", nrow(df_r), nrow(df_cli))
    ))
  }
  eq <- isTRUE(all.equal(df_r[cols], df_cli[cols], check.attributes = FALSE))
  if (!eq) {
    d <- all.equal(df_r[cols], df_cli[cols], check.attributes = FALSE)
    return(list(ok = FALSE, reason = paste(as.character(d), collapse = "; ")))
  }
  list(ok = TRUE, reason = "")
}

lines_report <- character()
append_report <- function(...) {
  s <- paste0(...)
  s <- sub("\n$", "", s)
  lines_report <<- c(lines_report, s)
}

fmt_sec <- function(x) {
  if (length(x) != 1L || is.na(x) || !is.finite(as.numeric(x))) {
    "-"
  } else {
    sprintf("%.3f", as.numeric(x))
  }
}

fmt_ratio_cli_over_r <- function(sec_r, sec_cli) {
  if (length(sec_r) != 1L || length(sec_cli) != 1L) {
    return("-")
  }
  sr <- as.numeric(sec_r)
  sc <- as.numeric(sec_cli)
  if (!is.finite(sr) || sr <= 0 || !is.finite(sc)) {
    "-"
  } else {
    sprintf("%.3f", sc / sr)
  }
}

results <- list()

parity_rud <- file.path(out_dir, "_parity_R_USER_CACHE")
dir.create(parity_rud, recursive = TRUE, showWarnings = FALSE)
prior_rud <- Sys.getenv("R_USER_CACHE_DIR", "")
Sys.setenv(R_USER_CACHE_DIR = parity_rud)
on.exit(
  {
    if (nzchar(prior_rud)) {
      Sys.setenv(R_USER_CACHE_DIR = prior_rud)
    } else {
      Sys.unsetenv("R_USER_CACHE_DIR")
    }
  },
  add = TRUE
)

cli_param_root <- if (isTRUE(legacy_txt_mode)) {
  legacy_txt_root
} else {
  parity_bundle_cli_root(normalizePath(zip_path, winslash = "/", mustWork = TRUE), out_dir)
}
writeLines(cli_param_root, file.path(out_dir, "CLI_PARAMETER_TABLE_ROOT.txt"))
if (isTRUE(args$legacy_input) && !isTRUE(legacy_txt_mode)) {
  legacy_flatten_bundle_tables(cli_param_root)
}

verbose <- isTRUE(args$verbose)

append_report("# R `shg_run` vs shg-cli parity\n\n")
append_report(sprintf("- **Generated:** %s\n", format(Sys.time(), usetz = TRUE)))
append_report(sprintf("- **shg-r root:** `%s`\n", repo_root))
append_report(sprintf(
  "- **Individuals (default per scenario):** %s (scenarios with `repeat_n` use that repeat instead)\n",
  format(parity_n, big.mark = ",")
))
append_report(sprintf("- **CLI binary:** `%s`\n", cli_bin))
append_report(sprintf("- **Parameter zip:** `%s`\n", zip_path))
append_report(sprintf("- **CLI table directory:** `%s`\n", cli_param_root))
append_report(sprintf("- **R_USER_CACHE_DIR:** `%s`\n", parity_rud))
append_report(sprintf("- **Scenarios:** %d\n", length(scenarios)))
if (isTRUE(legacy_txt_mode)) {
  append_report(
    "- **Legacy `.txt` tables:** `SHG_LEGACY_TXT_ROOT` → portable YAML round-trip skipped; ",
    "R and CLI read the same folder. Optional relative names: ",
    "`SHG_LEGACY_TXT_INITIATION`, `_CESSATION`, `_CPD`, `_ACM`, `_OCM` (defaults: ",
    "`initiation.txt`, …, `ocm-excl-lung-cancer.txt`).\n"
  )
}
if (isTRUE(args$legacy_input)) {
  append_report(
    "- **Legacy CLI input:** `TRUE` (",
    if (isTRUE(legacy_txt_mode)) {
      "flat `initiation.txt` / … under `SHG_LEGACY_TXT_ROOT`, "
    } else {
      "flat `initiation.csv` paths under the zip extract, "
    },
    "`OCD_PROB=` + `MORTALITY_PROB=`, duplicate `CESSATION_YEAR=`).\n"
  )
  if (isTRUE(args$legacy_rs_mt_seeds)) {
    append_report(
      "- **Legacy RS MT seeds:** RngStream scenarios also emit `SEED_INIT`…`SEED_MISC` ",
      "(`SHG_CLI_LEGACY_RS_MT_SEEDS`); use only for antique CLIs that require them.\n"
    )
  }
  append_report("\n")
} else {
  append_report("\n")
}
append_report(
  if (isTRUE(legacy_txt_mode)) {
    paste0(
      "Workflow per row (legacy `.txt`): `shg_load_params` from zip, override paths to `SHG_LEGACY_TXT_ROOT`, ",
      "timed `shg_run()` with a minimal config (no YAML round-trip), build matching `input.txt`, ",
      "timed CLI run, compare core columns. **R (s)** / **CLI (s)** / **CLI/R** as above. ",
      "Artifacts per scenario under `<out_dir>/<scenario_id>/`.\n\n"
    )
  } else {
    paste0(
      "Workflow per row: tune engine and save portable YAML, then `shg_load_config` + timed `shg_run`, ",
      "build matching `input.txt`, timed CLI run, compare `smoking_initiation_age`, `smoking_cessation_age`, ",
      "`age_at_death`. **R (s)** is wall time for `shg_run()` only; **CLI (s)** is wall time for the ",
      "CLI process. **CLI/R** is CLI seconds divided by R seconds (>1 means CLI slower than in-R sim). ",
      "Artifacts per scenario remain under `<out_dir>/<scenario_id>/` (add `--verbose` for YAML excerpts).\n\n"
    )
  }
)

append_report("## Results\n\n")
append_report("| Scenario | Parameters | Match | R (s) | CLI (s) | CLI/R |\n")
append_report("|----------|------------|-------|-------|---------|-------|\n")

apply_scenario_engine <- function(shg, sc) {
  shg$rng_strategy <- sc$rng_strategy
  if (identical(sc$rng_strategy, "RngStream")) {
    shg$rngstream_seed <- as.numeric(sc$rngstream_seed)
  } else {
    shg$mt_seeds <- as.numeric(sc$mt_seeds)
  }
  shg$num_threads <- as.integer(sc$num_threads)
  shg$number_of_segments <- as.integer(sc$number_of_segments)
  shg$immediate_cessation_year <- as.integer(sc$immediate_cessation_year)
  shg$cpd_format <- sc$cpd_format
  invisible(shg)
}

for (sc in scenarios) {
  subd <- file.path(out_dir, sc$id)
  dir.create(subd, recursive = TRUE, showWarnings = FALSE)
  yml <- file.path(subd, "run.yml")
  inp <- file.path(subd, "input.txt")
  outp <- file.path(subd, "cli_output.txt")
  errp <- file.path(subd, "cli_errors.txt")
  unlink(c(yml, inp, outp, errp))

  tryCatch(
    {
      scenario_repeat <- if (!is.null(sc[["repeat_n"]])) {
        as.integer(sc[["repeat_n"]])
      } else {
        parity_n
      }
      if (length(scenario_repeat) != 1L || is.na(scenario_repeat) || scenario_repeat < 1L) {
        stop("Invalid scenario repeat for id: ", sc$id, call. = FALSE)
      }

      if (isTRUE(legacy_txt_mode)) {
        shg_r <- new(shg_class)
        SmokingHistoryGenerator::shg_load_params(
          shg_r,
          url = zip_path,
          mortality = sc$mortality
        )
        legacy_apply_txt_table_paths(shg_r, legacy_txt_root_iface, sc$mortality, legacy_txt_specs)
        apply_scenario_engine(shg_r, sc)

        bundle_run <- list(
          "repeat" = scenario_repeat,
          race = as.integer(sc$race),
          sex = as.integer(sc$sex),
          cohort_year = as.integer(sc$cohort_year),
          immediate_cessation_year = as.integer(sc$immediate_cessation_year)
        )
        st_r <- system.time({
          sim_r <- SmokingHistoryGenerator::shg_run(
            shg_r,
            c(bundle_run, list(output_file = "")),
            attach_run_info = FALSE
          )
        })
        sec_r <- unname(st_r["elapsed"])

        legacy_txt_from_loaded_shg(
          shg_r, bundle_run, inp, outp, errp,
          cli_param_root = cli_param_root,
          legacy_input = args$legacy_input,
          legacy_rs_mt_seeds = args$legacy_rs_mt_seeds
        )
      } else {
        shg0 <- new(shg_class)
        SmokingHistoryGenerator::shg_load_params(
          shg0,
          url = zip_path,
          mortality = sc$mortality
        )
        apply_scenario_engine(shg0, sc)

        shg0$runSimFromFixedValues(
          scenario_repeat,
          as.integer(sc$race),
          as.integer(sc$sex),
          as.integer(sc$cohort_year)
        )
        SmokingHistoryGenerator::shg_save_config(shg0, yml, quiet = TRUE)

        shg_r <- new(shg_class)
        bundle <- SmokingHistoryGenerator::shg_load_config(shg_r, yml)
        st_r <- system.time({
          sim_r <- SmokingHistoryGenerator::shg_run(shg_r, bundle, attach_run_info = FALSE)
        })
        sec_r <- unname(st_r["elapsed"])

        shg_txt <- new(shg_class)
        bundle2 <- SmokingHistoryGenerator::shg_load_config(shg_txt, yml)

        legacy_txt_from_loaded_shg(
          shg_txt, bundle2, inp, outp, errp,
          cli_param_root = cli_param_root,
          legacy_input = args$legacy_input,
          legacy_rs_mt_seeds = args$legacy_rs_mt_seeds
        )
      }
      cli_res <- run_cli(inp)
      sec_cli <- cli_res$elapsed_sec

      r_core_csv <- file.path(subd, "r_core_results.csv")
      utils::write.csv(sim_r[, parity_core_cols, drop = FALSE], r_core_csv, row.names = FALSE)

      cli_parse_err <- NULL
      df_cli <- if (cli_res$rc == 0L && file.exists(outp)) {
        tryCatch(
          read_cli_data(outp),
          error = function(e) {
            cli_parse_err <<- conditionMessage(e)
            NULL
          }
        )
      } else {
        NULL
      }

      cli_core_csv <- file.path(subd, "cli_core_results.csv")
      if (!is.null(df_cli)) {
        utils::write.csv(df_cli[, parity_core_cols, drop = FALSE], cli_core_csv, row.names = FALSE)
      } else if (file.exists(cli_core_csv)) {
        unlink(cli_core_csv)
      }

      cmp <- if (!is.null(df_cli)) {
        compare_core(sim_r, df_cli)
      } else {
        list(ok = FALSE, reason = "missing CLI parse")
      }

      results[[sc$id]] <- list(
        scenario = sc,
        cli_rc = cli_res$rc,
        cli_stderr = cli_res$stderr,
        cli_parse_error = cli_parse_err,
        compare = cmp,
        setup_error = NULL,
        sec_r = sec_r,
        sec_cli = sec_cli,
        n_r = nrow(sim_r),
        n_cli = if (!is.null(df_cli)) nrow(df_cli) else NA_integer_,
        r_core_csv = r_core_csv,
        cli_core_csv = if (!is.null(df_cli)) cli_core_csv else NA_character_
      )
    },
    error = function(e) {
      msg <- conditionMessage(e)
      results[[sc$id]] <<- list(
        scenario = sc,
        cli_rc = NA_integer_,
        cli_stderr = "",
        cli_parse_error = NULL,
        compare = list(ok = FALSE, reason = msg),
        setup_error = msg,
        sec_r = NA_real_,
        sec_cli = NA_real_,
        n_r = NA_integer_,
        n_cli = NA_integer_,
        r_core_csv = NA_character_,
        cli_core_csv = NA_character_
      )
    }
  )

  r <- results[[sc$id]]
  match_cell <- if (isTRUE(r$compare$ok)) "Yes" else "No"
  ratio_cell <- fmt_ratio_cli_over_r(r$sec_r, r$sec_cli)
  append_report(sprintf(
    "| %s | %s | %s | %s | %s | %s |\n",
    sc$id,
    sc$params_cell,
    match_cell,
    fmt_sec(r$sec_r),
    fmt_sec(r$sec_cli),
    ratio_cell
  ))

  if (verbose) {
    append_report("\n### ", sc$id, "\n\n")
    append_report(sc$params_cell, "\n\n")
    append_report("| Artifact | Path |\n|----|----|\n")
    append_report(sprintf("| Portable YAML | `%s` |\n", yml))
    append_report(sprintf("| CLI legacy input | `%s` |\n", inp))
    append_report(sprintf("| CLI stdout | `%s` |\n", outp))
    if (file.exists(yml)) {
      append_report("\n```yaml\n")
      append_report(paste(readLines(yml, warn = FALSE), collapse = "\n"), "\n")
      append_report("```\n\n")
    }
    if (!is.null(r$setup_error)) {
      append_report("**Setup / R error**\n\n```\n", r$setup_error, "\n```\n\n")
    }
    if (!is.na(r$cli_rc) && r$cli_rc != 0L) {
      append_report(sprintf("**CLI exit code:** %s\n\n", r$cli_rc))
    }
    if (nzchar(r$cli_stderr)) {
      append_report("**CLI stderr (tail)**\n\n```\n")
      te <- r$cli_stderr
      if (nchar(te) > 4000) {
        te <- paste0("...\n", substr(te, nchar(te) - 4000L, nchar(te)))
      }
      append_report(te, "\n```\n\n")
    }
    if (!is.null(r$cli_parse_error)) {
      append_report("**CLI parse error**\n\n```\n", r$cli_parse_error, "\n```\n\n")
    }
    if (isTRUE(r$compare$ok)) {
      append_report("**Match:** Yes (`all.equal` on core columns).\n\n")
    } else {
      append_report("**Match:** No — ", r$compare$reason, "\n\n")
    }
  }
}

writeLines(lines_report, report_path, useBytes = TRUE)
message("Report written: ", report_path)
message("Artifacts under: ", out_dir)

summary_csv <- sub("\\.[mM][dD]$", "-summary.csv", report_path)
if (!nzchar(summary_csv) || summary_csv == report_path) {
  summary_csv <- file.path(out_dir, "parity_summary.csv")
}
summ_rows <- lapply(names(results), function(nm) {
  rr <- results[[nm]]
  data.frame(
    scenario_id = nm,
    match_ok = isTRUE(rr$compare$ok),
    reason = paste(as.character(rr$compare$reason), collapse = " "),
    n_r = rr$n_r,
    n_cli = rr$n_cli,
    cli_rc = rr$cli_rc,
    r_core_csv = rr$r_core_csv,
    cli_core_csv = rr$cli_core_csv,
    sec_r_s = rr$sec_r,
    sec_cli_s = rr$sec_cli,
    stringsAsFactors = FALSE
  )
})
summ_df <- do.call(rbind, summ_rows)
utils::write.csv(summ_df, summary_csv, row.names = FALSE)
message("Summary CSV: ", summary_csv)

r_stack <- list()
cli_stack <- list()
skip_stack <- tolower(trimws(Sys.getenv("SHG_CLI_PARITY_NO_STACK", ""))) %in%
  c("1", "true", "yes", "y")
if (!skip_stack) {
  for (nm in names(results)) {
    rr <- results[[nm]]
    rc <- rr$r_core_csv
    if (length(rc) == 1L && nzchar(rc) && file.exists(rc)) {
      z <- utils::read.csv(rc, stringsAsFactors = FALSE)
      z$scenario_id <- nm
      r_stack[[nm]] <- z
    }
    cc <- rr$cli_core_csv
    if (length(cc) == 1L && nzchar(cc) && !is.na(cc) && file.exists(cc)) {
      z2 <- utils::read.csv(cc, stringsAsFactors = FALSE)
      z2$scenario_id <- nm
      cli_stack[[nm]] <- z2
    }
  }
}
if (!skip_stack && length(r_stack)) {
  r_all <- do.call(rbind, r_stack)
  r_all_path <- file.path(out_dir, "parity_all_R_core_long.csv")
  utils::write.csv(r_all, r_all_path, row.names = FALSE)
  message("Stacked R core CSV: ", r_all_path)
}
if (!skip_stack && length(cli_stack)) {
  cli_all <- do.call(rbind, cli_stack)
  cli_all_path <- file.path(out_dir, "parity_all_cli_core_long.csv")
  utils::write.csv(cli_all, cli_all_path, row.names = FALSE)
  message("Stacked CLI core CSV: ", cli_all_path)
}
