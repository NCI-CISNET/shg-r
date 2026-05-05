# Config bundle: portable YAML save/load, useConfig, and run-from-config.

#' Build a config list suitable for inspection or advanced serialization
#'
#' Returns \code{shg$getConfig()}: engine fields, provenance, and run metadata
#' when available (e.g. after \code{\link{runSimFromFixedValues}}).
#'
#' @param shg An \code{SHGInterface} instance.
#' @return A plain list (see \code{\link{shg_save_config}} for portable YAML).
#' @seealso \code{\link{shg_save_config}}, \code{\link{shg_load_config}}, \code{\link{shg_run}}
#' @export
shg_config_bundle <- function(shg) {
  shg$getConfig()
}


#' Save a portable reproducibility config as YAML
#'
#' Writes a YAML file containing \code{params_bundle_source}, mortality choice,
#' engine settings (RNG, seeds, threads, segments), fixed-run parameters
#' (\code{repeat}, \code{race}, \code{sex}, \code{cohort_year}), and
#' \code{immediate_cessation_year}. Omits derived paths (\code{input_data_folder},
#' per-table filenames) so the bundle stays portable; those paths are restored by
#' \code{\link{shg_load_params}} when loading from \code{params_bundle_source}.
#'
#' @details
#' \strong{Prefer the method form} \code{shg$save_config(path)} (same implementation).
#' The functional form \code{shg_save_config(shg, path)} is a convenience wrapper.
#'
#' Saving reads \code{shg$getReproConfig(debug = FALSE)} after your workflow. Portable
#' save is allowed only when the \strong{last completed simulation} on this instance
#' was \code{\link{runSimFromFixedValues}} — a subsequent \code{runSimFromDataFrame}
#' (population run) clears that until you run \code{runSimFromFixedValues} again.
#' Use \code{shg$last_completed_sim_was_fixed_cohort()} to test programmatically.
#'
#' The run scalars (\code{repeat}, \code{race}, \code{sex}, \code{cohort_year}) come
#' from that fixed cohort run. Engine fields (\code{number_of_segments},
#' \code{num_threads}, \code{rng_strategy}, \code{seeds}) reflect \strong{effective}
#' values from it when you used defaults or auto settings (e.g.\ \code{num_threads = -1}).
#'
#' If the last run was not a fixed cohort simulation, or fixed cohort metadata are
#' missing, saving fails with an error.
#'
#' @param shg An \code{SHGInterface} instance.
#' @param path Destination file path (usually \code{.yml} or \code{.yaml}).
#' @param quiet If \code{TRUE}, suppress the explanatory message printed on save.
#' @return \code{path}, invisibly.
#' @seealso \code{\link{shg_load_config}}, \code{\link{shg_run}}
#' @examples
#' \dontrun{
#' shg <- new(SHGInterface)
#' shg$load_params(url = "/path/to/bundle.zip")
#' shg$runSimFromFixedValues(1000, 0, 0, 2010)
#' shg$save_config("my-run.yml")
#' # Alternate:
#' # shg_save_config(shg, "my-run.yml")
#' }
#' @export
shg_save_config <- function(shg, path, quiet = FALSE) {
  if (!isTRUE(shg$last_completed_sim_was_fixed_cohort())) {
    stop(
      "Cannot save portable config: the most recent completed simulation on this ",
      "instance must be runSimFromFixedValues(). If you ran runSimFromDataFrame() ",
      "(a population run) after your last fixed cohort run, run runSimFromFixedValues() ",
      "again before saving.",
      call. = FALSE
    )
  }
  cfg <- shg$getReproConfig(FALSE)
  portable <- .shg_portable_config_list(cfg)
  if (!isTRUE(quiet)) {
    message(
      "Portable YAML saved: configuration reflects the last completed runSimFromFixedValues() ",
      "(repeat, race, sex, cohort year, and effective engine settings)."
    )
  }
  yaml::write_yaml(portable, path)
  invisible(path)
}


#' Load engine state and parameters from a YAML config file
#'
#' Reads the YAML file, applies engine settings with \code{useConfig()}, then
#' restores parameter tables via \code{\link{shg_load_params}} when the cache is
#' missing or stale (using \code{params_bundle_source} and \code{params_mortality}
#' stored in the file).
#'
#' Private GitHub downloads use the \code{GITHUB_PAT} environment variable when
#' needed (same as \code{\link{shg_load_params}}).
#'
#' @param shg An \code{SHGInterface} instance.
#' @param path Path to a YAML file produced by \code{\link{shg_save_config}} or
#'   compatible hand-written YAML.
#' @return The parsed config list (same object to pass to \code{\link{shg_run}} /
#'   \code{runSim}). Return value is visible so you can assign:
#'   \code{config <- shg_load_config(shg, "my-run.yml")}.
#' @seealso \code{\link{shg_save_config}}, \code{\link{shg_run}}
#' @export
shg_load_config <- function(shg, path) {
  if (!is.character(path) || length(path) != 1 || is.na(path[1]) || !nzchar(path[1]))
    stop("'path' must be a single non-empty path to a YAML file.")
  path <- enc2utf8(path[1])
  if (!isTRUE(file.exists(path)))
    stop("YAML file not found: ", path)

  bundle <- yaml::read_yaml(path)
  if (!is.list(bundle) || length(bundle) == 0)
    stop("YAML did not parse to a non-empty mapping.")

  bundle <- .shg_normalize_config_list(bundle)
  .shg_apply_config_bundle(shg, bundle)
  bundle
}


#' Backwards-compatible alias for \code{shg_load_config}
#'
#' @rdname shg_load_config
#' @export
shg_use_config_bundle <- shg_load_config


#' Run a fixed cohort simulation from a config list
#'
#' Validates required keys and calls \code{\link{runSimFromFixedValues}}.
#' If \code{config} is a single string, it is treated as a path and read with
#' \code{\link[yaml]{read_yaml}} (use after \code{\link{shg_load_config}} with the
#' returned list is preferred).
#'
#' @param shg An \code{SHGInterface} instance.
#' @param config Named list from \code{\link{shg_load_config}}, or a YAML path.
#' @return A data frame from \code{runSimFromFixedValues}.
#' @seealso \code{\link{shg_load_config}}, \code{\link{shg_save_config}}
#' @export
shg_run <- function(shg, config) {
  .shg_run_sim(shg, config)
}


# Called from SHGInterface$runSim in zzz.R
.shg_run_sim <- function(shg, config) {
  if (missing(config) || is.null(config))
    stop("'config' must be a list from shg_load_config() or yaml::read_yaml().")

  if (is.character(config) && length(config) == 1 && nzchar(config[1])) {
    cf <- config[1]
    if (!isTRUE(file.exists(cf)))
      stop("Config file not found: ", cf)
    config <- yaml::read_yaml(cf)
    config <- .shg_normalize_config_list(config)
  }

  if (!is.list(config))
    stop("'config' must be a list or a path to YAML.")

  req <- c("repeat", "race", "sex", "cohort_year")
  miss <- character(0)
  for (f in req) {
    v <- config[[f]]
    if (is.null(v)) {
      miss <- c(miss, f)
      next
    }
    if (is.list(v))
      v <- unlist(v, use.names = FALSE)
    if (length(v) < 1 || is.na(v[1]))
      miss <- c(miss, f)
  }
  if (length(miss))
    stop("Config missing required run field(s): ", paste(miss, collapse = ", "))

  if (!is.null(config$immediate_cessation_year))
    shg$immediate_cessation_year <- as.integer(config$immediate_cessation_year)

  shg$runSimFromFixedValues(
    as.integer(config[["repeat"]]),
    as.integer(config[["race"]]),
    as.integer(config[["sex"]]),
    as.integer(config[["cohort_year"]])
  )
}


# ---------------------------------------------------------------------------
# Internal
# ---------------------------------------------------------------------------

.shg_apply_config_bundle <- function(shg, bundle) {
  meta_src <- bundle$params_bundle_source
  meta_mot <- bundle$params_mortality
  if (is.null(meta_mot) || (length(meta_mot) == 1 && is.na(meta_mot)))
    meta_mot <- "acm"
  else
    meta_mot <- match.arg(as.character(meta_mot), c("acm", "ocm"))

  cfg <- bundle
  cfg <- .shg_strip_derived_input_paths(cfg)
  cfg <- .shg_strip_bundle_for_useconfig(cfg)

  # Fresh instances default to package extdata; clear so params_paths_exist reflects
  # whether we have restored this bundle's extracted zip (see params_bundle_source).
  src  <- if (is.null(meta_src) || length(meta_src) != 1) "" else trimws(as.character(meta_src))
  if (nzchar(src) && !is.na(src))
    shg$input_data_folder <- ""

  shg$useConfig(cfg)

  need <- !.shg_params_paths_exist(shg)

  # Always apply params_bundle_source when present so we do not keep the package
  # default extdata folder from a fresh SHGInterface() instance.
  if (nzchar(src) && !is.na(src)) {
    if (need) {
      message("Parameter cache or paths missing; re-loading bundle from:\n  ", src)
      cache_path <- .shg_params_cache_path(src)
      if (dir.exists(cache_path))
        unlink(cache_path, recursive = TRUE)
    }
    shg_load_params(shg, url = src, mortality = meta_mot)
  } else if (need) {
    warning(
      "Configured paths are missing on disk and no params_bundle_source was saved; ",
      "call shg_load_params() first, or save with shg$save_config() after load_params()."
    )
  }

  invisible(TRUE)
}


.shg_strip_derived_input_paths <- function(bundle) {
  drop <- c(
    "input_data_folder", "initiation_filename", "cessation_filename",
    "mortality_filename", "cpd_filename"
  )
  i <- names(bundle) %in% drop
  bundle[!i]
}


.shg_strip_bundle_for_useconfig <- function(bundle) {
  drop <- c(
    "params_bundle_source", "params_mortality",
    "rng_state_fingerprint", "package_version", "package_source",
    "r_version", "platform", "memory_usage"
  )
  i <- names(bundle) %in% drop
  bundle[!i]
}


.shg_portable_config_list <- function(cfg) {
  drop <- c(
    "input_data_folder", "initiation_filename", "cessation_filename",
    "mortality_filename", "cpd_filename", "timestamp",
    "rng_state_fingerprint", "package_version", "package_source",
    "r_version", "platform", "memory_usage"
  )
  out <- cfg[!names(cfg) %in% drop]

  if (is.null(out$params_bundle_source) || length(out$params_bundle_source) != 1 ||
      is.na(out$params_bundle_source))
    stop(
      "Cannot save portable config: params_bundle_source is missing or NA. ",
      "Call shg_load_params() (or shg$load_params()) first."
    )

  req_run <- c("repeat", "race", "sex", "cohort_year")
  na_run <- character(0)
  for (nm in req_run) {
    v <- out[[nm]]
    if (is.null(v) || length(v) != 1 || is.na(v))
      na_run <- c(na_run, nm)
  }
  if (length(na_run)) {
    stop(
      "Cannot save portable config: no complete fixed cohort run is recorded on this instance ",
      "(missing or NA: ", paste(na_run, collapse = ", "), "). ",
      "Call runSimFromFixedValues() once before save_config(); the YAML stores that run's ",
      "repeat/race/sex/cohort_year and effective engine settings (segments, threads, RNG, seeds) ",
      "from after that simulation."
    )
  }

  out
}


.shg_normalize_config_list <- function(x) {
  if (!is.list(x))
    stop("Config must be a list from YAML.")
  if (is.null(names(x)) || any(names(x) == ""))
    stop("YAML root must be a named mapping (key/value pairs).")

  if (!is.null(x$seeds)) {
    s <- x$seeds
    if (is.list(s))
      s <- unlist(s, use.names = FALSE)
    s <- as.numeric(s)
    can_int <- length(s) > 0 && all(is.finite(s)) &&
      all(abs(s - round(s)) < 1e-9) &&
      all(s <= .Machine$integer.max & s >= -.Machine$integer.max)
    x$seeds <- if (can_int) as.integer(round(s)) else s
  }

  int_fields <- c(
    "number_of_segments", "num_threads", "immediate_cessation_year",
    "repeat", "race", "sex", "cohort_year"
  )
  for (f in int_fields) {
    if (is.null(x[[f]]))
      next
    v <- x[[f]]
    if (is.list(v))
      v <- unlist(v, use.names = FALSE)
    if (length(v) >= 1) {
      vv <- v[[1]]
      if (!is.na(vv))
        x[[f]] <- as.integer(vv)
    }
  }

  x
}


.shg_params_paths_exist <- function(shg) {
  root <- shg$input_data_folder
  if (!nzchar(root) || !dir.exists(root))
    return(FALSE)
  f <- file.path(root, shg$initiation_filename)
  file.exists(f)
}
