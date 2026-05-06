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
#
# Requires tests/testdata/usa-national@smok-2016.zip (or SHG_PARAMS_ZIP) for full
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
    verbose = FALSE
  )
  for (x in a) {
    if (grepl("^--out-dir=", x)) out$out_dir <- sub("^--out-dir=", "", x)
    if (grepl("^--report=", x)) out$report <- sub("^--report=", "", x)
    if (x == "--verbose") out$verbose <- TRUE
  }
  out
}

args <- parse_args()
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

zip_default <- file.path(repo_root, "tests", "testdata", "usa-national@smok-2016.zip")
zip_path <- Sys.getenv("SHG_PARAMS_ZIP", zip_default)
zip_path <- normalizePath(zip_path, winslash = "/", mustWork = FALSE)
if (!file.exists(zip_path)) {
  stop(
    "Parameter bundle not found: ", zip_path,
    "\nSet SHG_PARAMS_ZIP to usa-national@smok-2016.zip or another compatible zip.",
    call. = FALSE
  )
}

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
#' (directory that contains smoking/ and mortality/).
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

#' Build legacy RunWebVersion text from an SHGInterface after shg_load_config (paths + engine).
#' If \code{cli_param_root} is set, INIT/CESS/MORT/CPD lines use that folder (stable extract for
#' the CLI); otherwise \code{shg$input_data_folder} is used.
legacy_txt_from_loaded_shg <- function(
    shg,
    bundle,
    output_txt,
    output_data,
    error_txt,
    cli_param_root = NULL) {
  root <- if (length(cli_param_root) == 1L && nzchar(cli_param_root[1L])) {
    cli_param_root[1L]
  } else {
    shg$input_data_folder
  }
  gc <- shg$getConfig(debug = FALSE)
  rng <- gc$rng_strategy
  seeds <- unlist(gc$seeds, use.names = FALSE)
  seeds <- as.numeric(seeds)

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

  init <- abs_input_path(root, shg$initiation_filename)
  cess <- abs_input_path(root, shg$cessation_filename)
  mort <- abs_input_path(root, shg$mortality_filename)
  cpd <- abs_input_path(root, shg$cpd_filename)

  out_data <- normalizePath(output_data, winslash = "/", mustWork = FALSE)
  out_err <- normalizePath(error_txt, winslash = "/", mustWork = FALSE)

  lines <- c(
    paste0("RNGSTRATEGY=", rng),
    if (identical(rng, "RngStream")) {
      c(
        paste0("RNGSTREAM_SEED=", paste(as.integer(seeds), collapse = ",")),
        paste0("NUM_THREADS=", nt),
        paste0("NUM_SEGMENTS=", ns)
      )
    } else {
      c(
        paste0("SEED_INIT=", as.integer(seeds[1])),
        paste0("SEED_CESS=", as.integer(seeds[2])),
        paste0("SEED_MORTALITY=", as.integer(seeds[3])),
        paste0("SEED_MISC=", as.integer(seeds[4])),
        paste0("NUM_THREADS=", nt),
        paste0("NUM_SEGMENTS=", ns)
      )
    },
    "",
    paste0("RACE=", race),
    paste0("SEX=", sex),
    paste0("YOB=", yob),
    paste0("CESSATION_YR=", ic),
    paste0("REPEAT=", rep),
    "",
    paste0("INIT_PROB=", init),
    paste0("CESS_PROB=", cess),
    paste0("MORTALITY_PROB=", mort),
    paste0("CPD_DATA=", cpd),
    "",
    paste0("OUTPUTFILE=", out_data),
    paste0("ERRORFILE=", out_err)
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

compare_core <- function(df_r, df_cli) {
  cols <- c("smoking_initiation_age", "smoking_cessation_age", "age_at_death")
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

cli_param_root <- parity_bundle_cli_root(normalizePath(zip_path, winslash = "/", mustWork = TRUE), out_dir)
writeLines(cli_param_root, file.path(out_dir, "CLI_PARAMETER_TABLE_ROOT.txt"))

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
append_report(sprintf("- **Scenarios:** %d\n\n", length(scenarios)))
append_report(
  "Workflow per row: tune engine and save portable YAML, then `shg_load_config` + timed `shg_run`, ",
  "build matching `input.txt`, timed CLI run, compare `smoking_initiation_age`, `smoking_cessation_age`, ",
  "`age_at_death`. **R (s)** is wall time for `shg_run()` only; **CLI (s)** is wall time for the ",
  "CLI process. **CLI/R** is CLI seconds divided by R seconds (>1 means CLI slower than in-R sim). ",
  "Artifacts per scenario remain under `<out_dir>/<scenario_id>/` (add `--verbose` for YAML excerpts).\n\n"
)

append_report("## Results\n\n")
append_report("| Scenario | Parameters | Match | R (s) | CLI (s) | CLI/R |\n")
append_report("|----------|------------|-------|-------|---------|-------|\n")

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
      shg0 <- new(shg_class)
      SmokingHistoryGenerator::shg_load_params(
        shg0,
        url = zip_path,
        mortality = sc$mortality
      )
      shg0$rng_strategy <- sc$rng_strategy
      if (identical(sc$rng_strategy, "RngStream")) {
        shg0$rngstream_seed <- as.numeric(sc$rngstream_seed)
      } else {
        shg0$mt_seeds <- as.numeric(sc$mt_seeds)
      }
      shg0$num_threads <- as.integer(sc$num_threads)
      shg0$number_of_segments <- as.integer(sc$number_of_segments)
      shg0$immediate_cessation_year <- as.integer(sc$immediate_cessation_year)
      shg0$cpd_format <- sc$cpd_format

      scenario_repeat <- if (!is.null(sc[["repeat_n"]])) {
        as.integer(sc[["repeat_n"]])
      } else {
        parity_n
      }
      if (length(scenario_repeat) != 1L || is.na(scenario_repeat) || scenario_repeat < 1L) {
        stop("Invalid scenario repeat for id: ", sc$id, call. = FALSE)
      }

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

      legacy_txt_from_loaded_shg(shg_txt, bundle2, inp, outp, errp, cli_param_root = cli_param_root)
      cli_res <- run_cli(inp)
      sec_cli <- cli_res$elapsed_sec

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
        sec_cli = sec_cli
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
        sec_cli = NA_real_
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
