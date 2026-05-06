# Config bundle: portable YAML save/load, useConfig, and run-from-config.

#' @keywords internal
#' @noRd
.shg_build_run_info <- function(core_version = NA_character_) {
  pd <- utils::packageDescription("SmokingHistoryGenerator")
  if (!is.list(pd))
    pd <- as.list(pd)

  si <- Sys.info()
  cores <- tryCatch(
    as.integer(parallel::detectCores(TRUE)),
    error = function(e) NA_integer_
  )

  remote_fields <- c("RemoteType", "RemotePkgRef", "RemoteRepo", "RemoteUsername", "RemoteRef", "RemoteSha")
  prov <- list()
  for (nm in remote_fields) {
    v <- pd[[nm]]
    if (!is.null(v) && length(v) >= 1L && !is.na(v[1]))
      prov[[tolower(nm)]] <- as.character(v[1])
  }
  package_url <- pd$URL
  if (!is.null(package_url) && length(package_url) >= 1L)
    package_url <- as.character(package_url)[1]
  else
    package_url <- NA_character_

  list(
    executed_at = format(Sys.time(), "%Y-%m-%d %H:%M:%S", tz = "UTC"),
    host_platform = list(
      sysname = unname(si[["sysname"]]),
      release = unname(si[["release"]]),
      version = unname(si[["version"]]),
      machine = unname(si[["machine"]]),
      cpu_cores_detected = cores,
      r_version = R.version$version.string,
      r_platform = R.version$platform,
      os_type = .Platform$OS.type
    ),
    software_versions = list(
      shg_core_version = core_version,
      r_package_version = if (!is.null(pd$Version)) as.character(pd$Version)[1] else as.character(utils::packageVersion("SmokingHistoryGenerator")),
      r_wrapper_version = if (!is.null(pd$RWrapperVersion)) as.character(pd$RWrapperVersion)[1] else NA_character_,
      shg_engine_tag = if (!is.null(pd$SHGMostRecentTag)) as.character(pd$SHGMostRecentTag)[1] else NA_character_,
      shg_commit_hash = if (!is.null(pd$SHGCommitHash)) as.character(pd$SHGCommitHash)[1] else NA_character_,
      shg_src_hash = if (!is.null(pd$SHGsrcHash)) as.character(pd$SHGsrcHash)[1] else NA_character_
    ),
    package_provenance = c(
      list(package_url = package_url),
      prov
    )
  )
}

#' @keywords internal
#' @noRd
.shg_package_repro_identity <- function(core_version = NA_character_) {
  pd <- utils::packageDescription("SmokingHistoryGenerator")
  if (!is.list(pd))
    pd <- as.list(pd)

  pkg_root <- system.file(package = "SmokingHistoryGenerator")
  package_rds <- file.path(pkg_root, "Meta", "package.rds")
  install_fingerprint_md5 <- tryCatch({
    if (!file.exists(package_rds))
      NA_character_
    else
      as.character(unname(tools::md5sum(package_rds)[1]))
  }, error = function(e) NA_character_)

  remote_type <- if (!is.null(pd$RemoteType) && length(pd$RemoteType) >= 1L) {
    tolower(as.character(pd$RemoteType)[1])
  } else {
    NA_character_
  }
  source_type <- if (!is.na(remote_type) && nzchar(remote_type)) {
    paste0("remote:", remote_type)
  } else if (!is.null(pd$Repository) && length(pd$Repository) >= 1L && nzchar(as.character(pd$Repository)[1])) {
    paste0("repository:", as.character(pd$Repository)[1])
  } else {
    "local_or_unknown"
  }

  source_locator <- NA_character_
  if (!is.na(remote_type) && remote_type == "local") {
    if (!is.null(pd$RemotePkgRef) && length(pd$RemotePkgRef) >= 1L && nzchar(as.character(pd$RemotePkgRef)[1])) {
      source_locator <- as.character(pd$RemotePkgRef)[1]
    } else {
      source_locator <- find.package("SmokingHistoryGenerator")
    }
  } else if (!is.null(pd$RemoteRepo) && !is.null(pd$RemoteUsername) &&
             length(pd$RemoteRepo) >= 1L && length(pd$RemoteUsername) >= 1L &&
             nzchar(as.character(pd$RemoteRepo)[1]) && nzchar(as.character(pd$RemoteUsername)[1])) {
    ref <- if (!is.null(pd$RemoteRef) && length(pd$RemoteRef) >= 1L && nzchar(as.character(pd$RemoteRef)[1])) {
      as.character(pd$RemoteRef)[1]
    } else if (!is.null(pd$RemoteSha) && length(pd$RemoteSha) >= 1L && nzchar(as.character(pd$RemoteSha)[1])) {
      as.character(pd$RemoteSha)[1]
    } else {
      "HEAD"
    }
    source_locator <- paste0(
      "https://github.com/",
      as.character(pd$RemoteUsername)[1],
      "/",
      as.character(pd$RemoteRepo)[1],
      "@",
      ref
    )
  } else if (!is.null(pd$Repository) && length(pd$Repository) >= 1L && nzchar(as.character(pd$Repository)[1])) {
    source_locator <- as.character(pd$Repository)[1]
  } else {
    source_locator <- find.package("SmokingHistoryGenerator")
  }

  list(
    shg_core_version = core_version,
    r_package_version = if (!is.null(pd$Version)) as.character(pd$Version)[1] else as.character(utils::packageVersion("SmokingHistoryGenerator")),
    r_wrapper_version = if (!is.null(pd$RWrapperVersion)) as.character(pd$RWrapperVersion)[1] else NA_character_,
    shg_engine_tag = if (!is.null(pd$SHGMostRecentTag)) as.character(pd$SHGMostRecentTag)[1] else NA_character_,
    shg_commit_hash = if (!is.null(pd$SHGCommitHash)) as.character(pd$SHGCommitHash)[1] else NA_character_,
    shg_src_hash = if (!is.null(pd$SHGsrcHash)) as.character(pd$SHGsrcHash)[1] else NA_character_,
    source_type = source_type,
    source_locator = source_locator,
    install_fingerprint_md5 = install_fingerprint_md5
  )
}

#' @keywords internal
#' @noRd
.shg_warn_if_repro_mismatch <- function(expected_repro) {
  if (!is.list(expected_repro) || length(expected_repro) == 0L)
    return(invisible(FALSE))

  core_version <- expected_repro$shg_core_version
  if (is.null(core_version) || length(core_version) < 1L || is.na(core_version[1]))
    core_version <- NA_character_
  else
    core_version <- as.character(core_version[1])
  current <- .shg_package_repro_identity(core_version = core_version)

  has_md5 <- !is.null(expected_repro$install_fingerprint_md5) &&
    length(expected_repro$install_fingerprint_md5) == 1L &&
    !is.na(expected_repro$install_fingerprint_md5) &&
    nzchar(expected_repro$install_fingerprint_md5)

  if (isTRUE(has_md5)) {
    same_md5 <- identical(
      as.character(expected_repro$install_fingerprint_md5)[1],
      as.character(current$install_fingerprint_md5)[1]
    )
    if (!isTRUE(same_md5)) {
      warning(
        "Config package fingerprint differs from current installation. ",
        "Expected install_fingerprint_md5=", expected_repro$install_fingerprint_md5,
        ", current=", current$install_fingerprint_md5, ". ",
        "Reproducibility may differ if package source/build is not identical.",
        call. = FALSE
      )
      return(invisible(TRUE))
    }
    return(invisible(FALSE))
  }

  key_fields <- c("shg_core_version", "r_package_version", "r_wrapper_version", "source_locator")
  diffs <- character(0)
  for (k in key_fields) {
    ev <- expected_repro[[k]]
    cv <- current[[k]]
    if (!is.null(ev) && length(ev) >= 1L && !is.na(ev[1])) {
      if (is.null(cv) || length(cv) < 1L || is.na(cv[1]) || !identical(as.character(ev[1]), as.character(cv[1]))) {
        diffs <- c(diffs, k)
      }
    }
  }

  if (length(diffs)) {
    warning(
      "Config package identity differs from current installation (fields: ",
      paste(diffs, collapse = ", "), "). ",
      "Reproducibility may differ if package source/build is not identical.",
      call. = FALSE
    )
    return(invisible(TRUE))
  }
  invisible(FALSE)
}


#' Reset an SHG instance to factory defaults
#'
#' Restores the same engine fields as a freshly constructed
#' \code{\link{SHGInterface}} (package extdata paths, default RNG strategy,
#' auto segments/threads, cleared seeds and bundle provenance).
#'
#' @param shg An \code{SHGInterface} instance.
#' @return \code{shg}, invisibly.
#' @seealso \code{\link{shg_apply_config}}
#' @export
shg_reset_defaults <- function(shg) {
  shg$reset_to_factory_defaults()
  invisible(shg)
}


#' Apply a sparse or complete configuration (defaults first, then overlay)
#'
#' Resets the instance with \code{\link{shg_reset_defaults}}, then applies
#' \code{config}. When \code{params_bundle_source} is set, derived table paths are
#' stripped and parameters are restored via \code{\link{shg_load_params}} (same
#' idea as \code{\link{shg_load_config}}). Otherwise settings are applied with
#' \code{shg$useConfig()} only; explicit \code{input_data_folder} / per-table
#' filenames in \code{config} are preserved.
#'
#' @param shg An \code{SHGInterface} instance.
#' @param config Named list (may be empty).
#' @return \code{shg}, invisibly.
#' @seealso \code{\link{shg_reset_defaults}}, \code{\link{shg_load_config}}
#' @export
shg_apply_config <- function(shg, config = list()) {
  if (!is.list(config))
    stop("'config' must be a list.", call. = FALSE)
  cfg <- if (length(config) == 0L) list() else .shg_normalize_config_list(config)
  if (length(cfg) > 0L && is.null(cfg$config_version))
    cfg[["config_version"]] <- "1.0"
  repro_meta <- cfg$package_repro
  cfg$package_repro <- NULL
  meta_src <- cfg$params_bundle_source
  src <- ""
  if (!is.null(meta_src) && length(meta_src) == 1L) {
    s <- trimws(as.character(meta_src)[1])
    if (!is.na(s) && nzchar(s))
      src <- s
  }
  strip_paths <- nzchar(src)
  .shg_reset_then_apply_config_list(shg, cfg, repro_meta, strip_derived_paths = strip_paths)
  invisible(shg)
}


#' Write a configuration list to YAML
#'
#' Strips audit-only keys when present, then drops redundant input paths when
#' \code{params_bundle_source} is set (same idea as portable save). Sparse lists
#' serialize as-is (minimal keys only).
#'
#' For full portable fixed-cohort bundles, \code{config} should include
#' \code{params_bundle_source} and complete \code{repeat}, \code{race},
#' \code{sex}, \code{cohort_year} (see \code{\link{shg_save_config}}).
#'
#' @param config Named list (\code{original_config}, \code{repro_config}, or any intent list).
#' @param path Output file path.
#' @return \code{path}, invisibly.
#' @export
shg_write_config_yaml <- function(config, path) {
  if (!is.character(path) || length(path) != 1L || !nzchar(path[1]))
    stop("'path' must be a single non-empty file path.", call. = FALSE)
  if (!is.list(config))
    stop("'config' must be a list.", call. = FALSE)
  path <- enc2utf8(path[1])
  cfg <- .shg_sanitize_config_for_yaml(config)
  cfg <- .shg_normalize_for_yaml_write(cfg)
  cfg <- .shg_rename_repeat_to_individuals(cfg)
  yaml::write_yaml(cfg, path)
  invisible(path)
}


.shg_audit_field_names <- function() {
  c(
    "run_info", "results", "original_config", "repro_config",
    "executed_at", "host_platform", "software_versions", "package_provenance",
    "rng_state_fingerprint", "package_version", "package_source",
    "r_version", "platform", "memory_usage"
  )
}


.shg_sanitize_config_for_yaml <- function(cfg) {
  drop <- .shg_audit_field_names()
  cfg[!names(cfg) %in% drop]
}


.shg_normalize_for_yaml_write <- function(cfg) {
  drop_always <- c(
    "timestamp", "output_file",
    "rng_state_fingerprint", "package_version", "package_source",
    "r_version", "platform", "memory_usage"
  )
  out <- cfg[!names(cfg) %in% drop_always]

  pbs <- out$params_bundle_source
  has_bundle <- !is.null(pbs) && length(pbs) == 1L && !is.na(pbs) && nzchar(trimws(as.character(pbs)))
  if (isTRUE(has_bundle)) {
    path_drop <- c(
      "input_data_folder", "initiation_filename", "cessation_filename",
      "mortality_filename", "cpd_filename"
    )
    out <- out[!names(out) %in% path_drop]
  }
  out <- .shg_rename_repeat_to_individuals(out)
  out <- .shg_format_integer_scalars_for_yaml(out)
  out <- .shg_format_individuals_for_yaml(out)
  .shg_reorder_config_fields(out)
}


.shg_rename_repeat_to_individuals <- function(cfg) {
  if (!is.null(cfg[["repeat"]]) && is.null(cfg[["individuals"]]))
    cfg[["individuals"]] <- cfg[["repeat"]]
  if (!is.null(cfg[["N"]]) && is.null(cfg[["individuals"]]))
    cfg[["individuals"]] <- cfg[["N"]]
  cfg[["repeat"]] <- NULL
  cfg[["N"]] <- NULL
  cfg
}


.shg_format_individuals_for_yaml <- function(cfg) {
  n <- cfg[["individuals"]]
  if (is.null(n) || length(n) != 1L)
    return(cfg)
  n_num <- suppressWarnings(as.numeric(n))
  if (!is.finite(n_num))
    return(cfg)
  # For larger runs, store count in compact scientific notation for readability.
  if (abs(n_num) >= 1e5) {
    s <- format(n_num, scientific = TRUE, trim = TRUE)
    s <- tolower(s)
    s <- sub("e\\+0*", "e", s)
    s <- sub("e-0*", "e-", s)
    cfg[["individuals"]] <- s
  }
  cfg
}


.shg_format_integer_scalars_for_yaml <- function(cfg) {
  int_fields <- c(
    "individuals", "race", "sex", "cohort_year",
    "immediate_cessation_year", "number_of_segments", "num_threads"
  )
  for (nm in int_fields) {
    v <- cfg[[nm]]
    if (is.null(v) || length(v) != 1L || is.character(v) || is.list(v))
      next
    vv <- suppressWarnings(as.numeric(v[[1]]))
    if (!is.finite(vv))
      next
    if (abs(vv - round(vv)) < 1e-9)
      cfg[[nm]] <- as.integer(round(vv))
  }
  cfg
}


.shg_reorder_config_fields <- function(cfg) {
  preferred <- c(
    "config_version", "rng_strategy", "individuals",
    "race", "sex", "cohort_year", "immediate_cessation_year",
    "number_of_segments", "num_threads", "seeds",
    "params_bundle_source", "params_mortality",
    "package_repro"
  )
  front <- preferred[preferred %in% names(cfg)]
  tail <- setdiff(names(cfg), front)
  cfg[c(front, tail)]
}


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
  yaml::write_yaml(portable, path)
  if (!isTRUE(quiet)) {
    message(
      "Portable YAML saved: configuration reflects the last completed runSimFromFixedValues() ",
      "(repeat, race, sex, cohort year, and effective engine settings)."
    )
  }
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
#' If \code{repeat}, \code{individuals}, and \code{N} are all omitted,
#' \code{repeat} defaults to \code{1000L}.
#' If \code{config} is a single string, it is treated as a path and read with
#' \code{\link[yaml]{read_yaml}} (use after \code{\link{shg_load_config}} with the
#' returned list is preferred).
#'
#' @param shg An \code{SHGInterface} instance.
#' @param config Named list from \code{\link{shg_load_config}}, or a YAML path.
#' @param attach_run_info If \code{TRUE} (default), returns a list with
#'   \code{results}, \code{original_config}, \code{repro_config}, and
#'   \code{run_info}. Set to \code{FALSE} to return only the simulation
#'   \code{data.frame}.
#' @return A data frame from \code{runSimFromFixedValues}, or a bundle list when
#'   \code{attach_run_info} is \code{TRUE}.
#' @seealso \code{\link{shg_load_config}}, \code{\link{shg_save_config}}
#' @export
shg_run <- function(shg, config, attach_run_info = TRUE) {
  .shg_run_sim(shg, config, attach_run_info)
}


# Called from SHGInterface$runSim in zzz.R
.shg_run_sim <- function(shg, config, attach_run_info = TRUE) {
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

  bundle_src <- config[["params_bundle_source"]]
  has_bundle_src <- !is.null(bundle_src) &&
    length(bundle_src) == 1L &&
    !is.na(bundle_src[[1]]) &&
    nzchar(trimws(as.character(bundle_src[[1]])))
  if (isTRUE(has_bundle_src)) {
    # One-step run for config-first workflows: hydrate params and apply config
    # before simulation when bundle provenance is supplied in run config.
    shg_apply_config(shg, config)
  }

  original_config <- config
  if (!is.null(original_config[["mortality"]]) && is.null(original_config[["params_mortality"]]))
    original_config[["params_mortality"]] <- original_config[["mortality"]]
  original_config[["mortality"]] <- NULL

  if (is.null(config[["repeat"]]) && !is.null(config[["individuals"]]))
    config[["repeat"]] <- config[["individuals"]]
  if (is.null(config[["repeat"]]) && !is.null(config[["N"]]))
    config[["repeat"]] <- config[["N"]]
  if (is.null(config[["repeat"]]))
    config[["repeat"]] <- 1000L
  if (is.null(config[["race"]]))
    config[["race"]] <- 0L
  if (is.null(config[["sex"]]))
    config[["sex"]] <- 0L

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

  output_file <- config[["output_file"]]
  if (is.null(output_file) || length(output_file) != 1L || is.na(output_file[[1]]))
    output_file <- ""
  output_file <- as.character(output_file[[1]])

  shg$runSimFromFixedValues(
    as.integer(config[["repeat"]]),
    as.integer(config[["race"]]),
    as.integer(config[["sex"]]),
    as.integer(config[["cohort_year"]]),
    attach_run_info,
    original_config,
    output_file
  )
}


# ---------------------------------------------------------------------------
# Internal
# ---------------------------------------------------------------------------

.shg_apply_config_bundle <- function(shg, bundle) {
  repro_meta <- bundle$package_repro
  bundle$package_repro <- NULL
  .shg_reset_then_apply_config_list(shg, bundle, repro_meta, strip_derived_paths = TRUE)
}


#' @keywords internal
#' @noRd
.shg_reset_then_apply_config_list <- function(shg,
                                               cfg,
                                               repro_meta,
                                               strip_derived_paths) {
  meta_src <- cfg$params_bundle_source
  meta_mot_raw <- cfg$params_mortality
  if (is.null(meta_mot_raw) || length(meta_mot_raw) != 1L ||
      is.na(meta_mot_raw[1]) || !nzchar(trimws(as.character(meta_mot_raw)[1]))) {
    meta_mot <- "acm"
  } else {
    meta_mot <- match.arg(trimws(as.character(meta_mot_raw)[1]), c("acm", "ocm"))
  }

  src <- ""
  if (!is.null(meta_src) && length(meta_src) == 1L) {
    s <- trimws(as.character(meta_src)[1])
    if (!is.na(s) && nzchar(s))
      src <- s
  }

  shg$reset_to_factory_defaults()

  if (nzchar(src) && !is.na(src)) {
    shg$params_bundle_source <- src
    shg$params_mortality <- meta_mot
  }

  cfg_use <- cfg
  if (isTRUE(strip_derived_paths))
    cfg_use <- .shg_strip_derived_input_paths(cfg_use)
  cfg_use <- .shg_strip_bundle_for_useconfig(cfg_use)

  if (nzchar(src) && !is.na(src))
    shg$input_data_folder <- ""

  shg$useConfig(cfg_use)

  need <- !.shg_params_paths_exist(shg)

  if (nzchar(src) && !is.na(src)) {
    if (need) {
      cache_path <- .shg_params_cache_path(src)
      cache_intact <- dir.exists(cache_path) && {
        snap <- tryCatch(.shg_snapshot_root(cache_path), error = function(e) cache_path)
        file.exists(file.path(snap, "smoking", "initiation.csv"))
      }
      if (!cache_intact) {
        message("Parameter cache or paths missing; re-loading bundle from:\n  ", src)
        if (dir.exists(cache_path))
          unlink(cache_path, recursive = TRUE)
      }
    }
    shg_load_params(shg, url = src, mortality = meta_mot)
  } else if (need) {
    warning(
      "Configured paths are missing on disk and no params_bundle_source was saved; ",
      "call shg_load_params() first, or save with shg$save_config() after load_params()."
    )
  }

  .shg_warn_if_repro_mismatch(repro_meta)
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
    "params_bundle_source", "params_mortality", "mortality",
    "N", "individuals",
    "package_repro",
    "rng_state_fingerprint", "package_version", "package_source",
    "r_version", "platform", "memory_usage"
  )
  i <- names(bundle) %in% drop
  bundle[!i]
}


.shg_portable_config_list <- function(cfg) {
  if (is.null(cfg[["repeat"]]) && !is.null(cfg[["individuals"]]))
    cfg[["repeat"]] <- cfg[["individuals"]]
  if (is.null(cfg[["repeat"]]) && !is.null(cfg[["N"]]))
    cfg[["repeat"]] <- cfg[["N"]]

  drop <- c(
    "input_data_folder", "initiation_filename", "cessation_filename",
    "mortality_filename", "cpd_filename", "timestamp", "output_file",
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

  out <- .shg_rename_repeat_to_individuals(out)
  out <- .shg_format_individuals_for_yaml(out)
  .shg_reorder_config_fields(out)
}


.shg_normalize_config_list <- function(x) {
  if (!is.list(x))
    stop("Config must be a list from YAML.")
  if (is.null(names(x)) || any(names(x) == ""))
    stop("YAML root must be a named mapping (key/value pairs).")

  if (is.null(x[["repeat"]]) && !is.null(x[["individuals"]]))
    x[["repeat"]] <- x[["individuals"]]
  if (is.null(x[["repeat"]]) && !is.null(x[["N"]]))
    x[["repeat"]] <- x[["N"]]
  x[["individuals"]] <- NULL
  x[["N"]] <- NULL

  if (is.null(x$params_mortality) || (length(x$params_mortality) == 1 && is.na(x$params_mortality))) {
    mo <- x[["mortality"]]
    if (!is.null(mo))
      x$params_mortality <- mo
  }
  x[["mortality"]] <- NULL

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
      vv_num <- suppressWarnings(as.numeric(vv))
      if (!is.na(vv_num))
        x[[f]] <- as.integer(round(vv_num))
    }
  }

  x
}


.shg_params_paths_exist <- function(shg) {
  root <- shg$input_data_folder
  if (!nzchar(root) || !dir.exists(root))
    return(FALSE)
  rel <- trimws(
    c(
      as.character(shg$initiation_filename),
      as.character(shg$cessation_filename),
      as.character(shg$cpd_filename),
      as.character(shg$mortality_filename)
    )
  )
  if (length(rel) != 4L || any(is.na(rel)) || any(!nzchar(rel)))
    return(FALSE)
  paths <- file.path(root, rel)
  isTRUE(all(file.exists(paths)))
}
