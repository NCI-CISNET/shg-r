#' Summarize currently configured SHG parameter tables
#'
#' Returns a compact "shape" summary of the currently configured parameter files
#' (races, sexes, cohorts, age ranges, and CPD coverage). This works after
#' either [shg_load_params()] or manual file-path configuration on an
#' \code{SHGInterface} instance.
#'
#' @param shg An \code{SHGInterface} instance.
#' @return A named list with nested sections \code{initiation},
#'   \code{cessation}, \code{mortality}, and \code{cpd}, plus
#'   top-level dimensions/cohort metadata for convenience.
#'
#'   The \code{cpd$note} field summarizes whether initiation rows below the
#'   CPD minimum age are effectively ignorable (all zeros and/or dots), or if
#'   there are non-zero initiation values that may indicate a mismatch.
#' @examples
#' \dontrun{
#' shg <- new(SHGInterface)
#' shg$load_params(url = "/path/to/usa-national@smok-2016.zip")
#' shg_params_summary(shg)
#'
#' # manual paths also work
#' shg$input_data_folder <- "/path/to/params-root"
#' shg$initiation_filename <- "smoking/initiation.csv"
#' shg$cessation_filename <- "smoking/cessation.csv"
#' shg$mortality_filename <- "mortality/acm.csv"
#' shg$cpd_filename <- "smoking/cpd.csv"
#' shg_params_summary(shg)
#' }
#' @export
shg_params_summary <- function(shg) {
  s <- shg$get_data_shape()
  init_path <- file.path(shg$input_data_folder, shg$initiation_filename)
  cess_path <- file.path(shg$input_data_folder, shg$cessation_filename)
  mort_path <- file.path(shg$input_data_folder, shg$mortality_filename)
  cpd_path <- file.path(shg$input_data_folder, shg$cpd_filename)

  init_profile <- .shg_table_profile(init_path, "initiation")
  cess_profile <- .shg_table_profile(cess_path, "cessation")
  mort_profile <- .shg_table_profile(mort_path, "mortality")
  cpd_profile <- .shg_table_profile(cpd_path, "cpd")
  cpd_note <- .shg_cpd_initiation_note(init_path, s$cpd_ages[["min"]])

  out <- list(
    num_races = s$num_races,
    num_sexes = s$num_sexes,
    num_cohorts = s$num_cohorts,
    cohort_start_years = s$cohort_start_years,
    cohort_end_years = s$cohort_end_years,
    first_cohort = s$first_cohort,
    last_cohort = s$last_cohort,
    initiation = list(
      ages = s$initiation_ages,
      cohorts = init_profile$cohorts,
      num_races = init_profile$num_races,
      num_sexes = init_profile$num_sexes
    ),
    cessation = list(
      ages = s$cessation_ages,
      cohorts = cess_profile$cohorts,
      num_races = cess_profile$num_races,
      num_sexes = cess_profile$num_sexes
    ),
    mortality = list(
      ages = s$mortality_ages,
      years = s$mortality_years,
      cohorts = mort_profile$cohorts,
      num_races = mort_profile$num_races,
      num_sexes = mort_profile$num_sexes
    ),
    cpd = list(
      ages = s$cpd_ages,
      cohorts = cpd_profile$cohorts,
      num_races = cpd_profile$num_races,
      num_sexes = cpd_profile$num_sexes,
      num_intensity_groups = s$num_intensity_groups,
      rows_loaded = s$cpd_rows_loaded,
      rows_skipped = s$cpd_rows_skipped,
      note = cpd_note[["note"]],
      initiation_alignment = cpd_note[["details"]]
    ),
    # Backward-compatible flat aliases
    initiation_ages = s$initiation_ages,
    cessation_ages = s$cessation_ages,
    mortality_ages = s$mortality_ages,
    mortality_years = s$mortality_years,
    cpd_ages = s$cpd_ages,
    num_intensity_groups = s$num_intensity_groups,
    cpd_rows_loaded = s$cpd_rows_loaded,
    cpd_rows_skipped = s$cpd_rows_skipped
  )

  out
}

.shg_cohort_summary <- function(values, windows = NULL) {
  vals <- sort(unique(as.integer(values)))
  vals <- vals[!is.na(vals)]
  out <- list(
    min = if (length(vals)) min(vals) else NA_integer_,
    max = if (length(vals)) max(vals) else NA_integer_,
    count = as.integer(length(vals)),
    values = vals
  )
  if (!is.null(windows))
    out$windows <- windows
  out
}


.shg_table_profile <- function(path, table) {
  out <- list(
    num_races = NA_integer_,
    num_sexes = NA_integer_,
    cohorts = .shg_cohort_summary(integer(0))
  )
  if (!is.character(path) || length(path) != 1 || !nzchar(path) || !file.exists(path))
    return(out)

  dat <- tryCatch(
    utils::read.csv(path, stringsAsFactors = FALSE, check.names = FALSE),
    error = function(e) NULL
  )
  if (!is.data.frame(dat))
    return(out)

  if ("RACE" %in% names(dat)) {
    rv <- suppressWarnings(as.integer(dat$RACE))
    out$num_races <- as.integer(length(unique(rv[!is.na(rv)])))
  }
  if ("SEX" %in% names(dat)) {
    sv <- suppressWarnings(as.integer(dat$SEX))
    out$num_sexes <- as.integer(length(unique(sv[!is.na(sv)])))
  }

  if (table %in% c("initiation", "cessation")) {
    cohort_cols <- setdiff(names(dat), c("RACE", "SEX", "AGE"))
    cohorts <- suppressWarnings(as.integer(cohort_cols))
    out$cohorts <- .shg_cohort_summary(cohorts)
    return(out)
  }

  if (table == "mortality") {
    if ("YOB" %in% names(dat)) {
      out$cohorts <- .shg_cohort_summary(dat$YOB)
      return(out)
    }
    if (all(c("START_YOB", "END_YOB") %in% names(dat))) {
      starts <- suppressWarnings(as.integer(dat$START_YOB))
      ends <- suppressWarnings(as.integer(dat$END_YOB))
      pairs <- unique(data.frame(start = starts, end = ends))
      pairs <- pairs[!is.na(pairs$start) & !is.na(pairs$end), , drop = FALSE]
      out$cohorts <- .shg_cohort_summary(c(starts, ends), windows = pairs)
      return(out)
    }
    cohort_cols <- setdiff(names(dat), c("RACE", "SEX", "AGE"))
    cohorts <- suppressWarnings(as.integer(cohort_cols))
    out$cohorts <- .shg_cohort_summary(cohorts)
    return(out)
  }

  if (table == "cpd") {
    if (all(c("START_YOB", "END_YOB") %in% names(dat))) {
      starts <- suppressWarnings(as.integer(dat$START_YOB))
      ends <- suppressWarnings(as.integer(dat$END_YOB))
      windows <- unique(data.frame(start = starts, end = ends))
      windows <- windows[!is.na(windows$start) & !is.na(windows$end), , drop = FALSE]
      # Cohort count for CPD is by START/END window pairs.
      out$cohorts <- .shg_cohort_summary(c(starts, ends), windows = windows)
      out$cohorts$count <- as.integer(nrow(windows))
      return(out)
    }
    cohort_cols <- setdiff(names(dat), c("RACE", "SEX", "AGE"))
    cohorts <- suppressWarnings(as.integer(cohort_cols))
    out$cohorts <- .shg_cohort_summary(cohorts)
    return(out)
  }

  out
}


.shg_cpd_initiation_note <- function(initiation_path, cpd_min_age) {
  unavailable <- list(
    note = "CPD/initiation alignment note unavailable (could not parse initiation table).",
    details = list(
      status = "unavailable",
      cpd_min_age = as.integer(cpd_min_age)
    )
  )
  if (!is.character(initiation_path) || length(initiation_path) != 1 ||
      !nzchar(initiation_path) || !file.exists(initiation_path))
    return(unavailable)
  if (is.null(cpd_min_age) || length(cpd_min_age) < 1 || is.na(cpd_min_age))
    return(unavailable)

  dat <- tryCatch(
    utils::read.csv(
      initiation_path,
      stringsAsFactors = FALSE,
      check.names = FALSE
    ),
    error = function(e) NULL
  )
  if (is.null(dat) || !is.data.frame(dat))
    return(unavailable)
  if (!all(c("RACE", "SEX", "AGE") %in% names(dat)))
    return(unavailable)

  cohort_cols <- setdiff(names(dat), c("RACE", "SEX", "AGE"))
  if (!length(cohort_cols))
    return(unavailable)

  age_cut <- as.integer(cpd_min_age[[1]])
  early <- dat[dat$AGE < age_cut, c("RACE", "SEX", "AGE", cohort_cols), drop = FALSE]
  if (!nrow(early)) {
    return(list(
      note = paste0(
        "CPD starts at age ", age_cut, "; there are no initiation rows below this age."
      ),
      details = list(
        status = "ok",
        cpd_min_age = age_cut,
        checked_ages = integer(0),
        groups_all_zero_or_dot = character(0),
        groups_with_nonzero = character(0),
        groups_with_dot_values = character(0)
      )
    ))
  }

  grp <- paste0("race=", early$RACE, ",sex=", early$SEX)
  vals <- as.matrix(early[, cohort_cols, drop = FALSE])
  vals_chr <- trimws(as.character(vals))
  vals_num <- suppressWarnings(as.numeric(vals_chr))
  is_dot <- vals_chr %in% c(".", "", "NA", "NaN")
  is_zero <- !is.na(vals_num) & abs(vals_num) < 1e-14
  is_nonzero <- !is.na(vals_num) & abs(vals_num) >= 1e-14

  group_stats <- lapply(split(seq_len(nrow(early)), grp), function(ix) {
    list(
      has_nonzero = any(is_nonzero[ix, , drop = FALSE], na.rm = TRUE),
      has_zero = any(is_zero[ix, , drop = FALSE], na.rm = TRUE),
      has_dot = any(is_dot[ix, , drop = FALSE], na.rm = TRUE)
    )
  })

  all_groups <- names(group_stats)
  groups_with_nonzero <- all_groups[vapply(group_stats, `[[`, logical(1), "has_nonzero")]
  groups_with_dot <- all_groups[vapply(group_stats, `[[`, logical(1), "has_dot")]
  groups_all_zero_or_dot <- setdiff(all_groups, groups_with_nonzero)

  age_min <- min(early$AGE, na.rm = TRUE)
  age_max <- max(early$AGE, na.rm = TRUE)
  age_label <- if (age_min == age_max) as.character(age_min) else paste0(age_min, "-", age_max)

  if (!length(groups_with_nonzero)) {
    if (length(groups_with_dot)) {
      note <- paste0(
        "CPD starts at age ", age_cut, ". For ages ", age_label,
        ", initiation values are 0 or '.' in all checked race/sex groups, ",
        "so those ages are effectively ignored for CPD alignment ('.' treated as missing)."
      )
    } else {
      note <- paste0(
        "CPD starts at age ", age_cut, ". For ages ", age_label,
        ", initiation values are 0 in all checked race/sex groups, so those ages ",
        "are effectively ignored for CPD alignment."
      )
    }
    status <- "ok"
  } else {
    note <- paste0(
      "CPD starts at age ", age_cut, ", but non-zero initiation values were found ",
      "below that age for: ", paste(groups_with_nonzero, collapse = "; "), "."
    )
    status <- "needs-review"
  }

  list(
    note = note,
    details = list(
      status = status,
      cpd_min_age = age_cut,
      checked_ages = sort(unique(as.integer(early$AGE))),
      groups_all_zero_or_dot = groups_all_zero_or_dot,
      groups_with_nonzero = groups_with_nonzero,
      groups_with_dot_values = groups_with_dot
    )
  )
}
