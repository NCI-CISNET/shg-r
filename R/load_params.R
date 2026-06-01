# SHG parameter set loading: download, cache, and configure an SHGInterface.
#
# The public surface is:
#   shg$load_params(...)        - method attached to SHGInterface in zzz.R
#   shg_load_params(shg, ...)   - same logic as a standalone function
#   shg_clear_params_cache()    - remove entire parameter cache directory
#   shg_params_cache_dir()      - inspect the cache location (same as shg$params_cache_dir)

# ---------------------------------------------------------------------------
# Public functions
# ---------------------------------------------------------------------------

#' Load SHG smoking and mortality parameter bundles and configure the instance
#'
#' @description
#' Downloads (or reuses locally cached copies of) separate **shg-params** smoking
#' and mortality release zips, merges them into an engine layout under the cache,
#' and sets `input_data_folder` plus relative input filenames on the
#' `SHGInterface` instance.
#'
#' Each zip uses the **shg-params** release layout (`params/` CSVs plus
#' `metadata.yml`). The simulator expects `smoking/*.csv` and `mortality/*.csv`
#' under one folder; this function materializes that tree from the two zips.
#'
#' @param shg An `SHGInterface` instance.
#' @param smoking_url URL or local path to the smoking `.zip` bundle.
#' @param mortality_url URL or local path to the mortality `.zip` bundle.
#' @param mort_params_type `"acm"` (**default**) or `"ocm"`.
#'
#' For private GitHub-hosted zips, set \code{GITHUB_PAT} before downloading.
#'
#' @section Download timeouts:
#' Options \code{shg.params.download.timeout_sec} (default 600) and
#' \code{shg.params.download.connect_sec} (default 60) control HTTPS transfers
#' when \pkg{httr2} is installed.
#'
#' @return The `SHGInterface` instance, invisibly.
#' @export
shg_load_params <- function(shg,
                            smoking_url = NULL,
                            mortality_url = NULL,
                            mort_params_type = c("acm", "ocm")) {
  mort_params_type <- match.arg(mort_params_type)
  smok_src <- .shg_require_param_source(smoking_url, "smoking_url")
  mort_src <- .shg_require_param_source(mortality_url, "mortality_url")

  combined_path <- .shg_ensure_combined_params_cache(smok_src, mort_src, mort_params_type)
  .shg_apply_params(shg, combined_path, mort_params_type)
  shg$smok_params_source <- as.character(smok_src)
  shg$mort_params_source <- as.character(mort_src)
  shg$mort_params_type <- as.character(mort_params_type)
  invisible(shg)
}


#' Return the directory where downloaded parameter sets are cached
#'
#' @return A length-one \code{character} path (visible). Same location as the
#'   read-only \code{params_cache_dir} field on \code{SHGInterface}. Extracted
#'   smoking and mortality bundles from \code{\link{shg_load_params}} are stored
#'   under this directory (via \code{tools::R_user_dir(..., "cache")}).
#' @export
shg_params_cache_dir <- function() {
  tools::R_user_dir("SmokingHistoryGenerator", "cache")
}


#' Clear the SHG parameter cache
#'
#' @return Invisibly, the cache path that was removed (\code{character}, length one),
#'   or \code{character()} if the directory did not exist (a message is printed in
#'   that case). Called for side effects when clearing disk cache; return value is
#'   mainly for scripting.
#' @export
shg_clear_params_cache <- function() {
  cache_dir <- shg_params_cache_dir()
  if (!dir.exists(cache_dir)) {
    message("Cache directory does not exist: ", cache_dir)
    return(invisible(character()))
  }
  unlink(cache_dir, recursive = TRUE)
  message("Cleared parameter cache: ", cache_dir)
  invisible(cache_dir)
}

#' @rdname shg_clear_params_cache
#' @export
clear_params_cache <- shg_clear_params_cache


# ---------------------------------------------------------------------------
# Internal helpers
# ---------------------------------------------------------------------------

.shg_require_param_source <- function(x, arg_name) {
  if (is.null(x) || length(x) != 1L || is.na(x) || !nzchar(trimws(as.character(x))))
    stop("'", arg_name, "' must be a single non-empty URL or local zip path.", call. = FALSE)
  trimws(as.character(x))
}

.shg_reject_legacy_combined_zip <- function(cache_path) {
  snap <- tryCatch(.shg_snapshot_root(cache_path), error = function(e) cache_path)
  if (dir.exists(file.path(snap, "smoking")) && dir.exists(file.path(snap, "mortality")) &&
      file.exists(file.path(snap, "smoking", "initiation.csv"))) {
    stop(
      "This looks like a legacy combined parameter zip (smoking/ + mortality/ at root). ",
      "Use separate shg-params smoking and mortality release zips with ",
      "smok_params_source and mort_params_source instead.",
      call. = FALSE
    )
  }
}

.shg_url_cache_key <- function(url) {
  seg <- sub("\\.zip$", "", basename(url))
  seg <- gsub("[^A-Za-z0-9._@-]", "_", seg)
  tf <- tempfile()
  on.exit(unlink(tf), add = TRUE)
  writeLines(url, tf)
  h <- substring(tools::md5sum(tf)[[1]], 1, 8)
  paste0(seg, "_", h)
}

.shg_params_cache_path <- function(url) {
  file.path(shg_params_cache_dir(), .shg_url_cache_key(url))
}

.shg_params_combined_cache_path <- function(smok_src, mort_src) {
  key <- paste0(
    "combined_",
    .shg_url_cache_key(smok_src),
    "__",
    .shg_url_cache_key(mort_src)
  )
  file.path(shg_params_cache_dir(), key)
}

.shg_resolve_token <- function(token, url) {
  if (!is.null(token) && nzchar(token)) return(token)
  if (grepl("github.com", url, fixed = TRUE)) {
    pat <- Sys.getenv("GITHUB_PAT", "")
    if (nzchar(pat)) return(pat)
  }
  NULL
}

.shg_is_local_path <- function(url) {
  !grepl("^https?://", url)
}

.shg_ensure_zip_extracted <- function(src, cache_path) {
  if (dir.exists(cache_path)) {
    message("Using cached parameter set:\n  ", cache_path)
    return(invisible(cache_path))
  }
  if (.shg_is_local_path(src)) {
    .shg_extract_local(src, cache_path)
  } else {
    .shg_download_and_extract(src, cache_path, NULL)
  }
  invisible(cache_path)
}

.shg_extract_local <- function(zip_path, cache_path) {
  if (!file.exists(zip_path))
    stop("Local parameter zip not found: ", zip_path, call. = FALSE)
  dir.create(cache_path, recursive = TRUE)
  message("Extracting local parameter set:\n  ", zip_path)
  .shg_assert_downloaded_zip(zip_path, zip_path)
  tryCatch(
    utils::unzip(zip_path, exdir = cache_path),
    error = function(e) {
      unlink(cache_path, recursive = TRUE)
      stop("Extraction failed for ", zip_path, ": ", conditionMessage(e), call. = FALSE)
    }
  )
  message("Cached at:\n  ", cache_path)
}

.shg_download_options <- function() {
  t <- getOption("shg.params.download.timeout_sec", 600L)
  csec <- getOption("shg.params.download.connect_sec", 60L)
  list(
    timeout_sec = max(as.numeric(t), 1),
    connect_sec = max(as.numeric(csec), 1)
  )
}

.shg_peek_file_raw <- function(path, n = 512L) {
  con <- file(path, "rb")
  on.exit(close(con), add = TRUE)
  readBin(con, "raw", n = as.integer(n))
}

.shg_assert_downloaded_zip <- function(path, url_for_message) {
  info <- file.info(path)
  if (is.na(info$size) || info$size == 0L) {
    stop(
      "Download saved an empty file - check the URL, authentication, and network.\n",
      "  URL: ", url_for_message,
      call. = FALSE
    )
  }
  raw <- .shg_peek_file_raw(path)
  if (length(raw) == 0L) {
    stop("Download is unreadable (empty read).\n  URL: ", url_for_message, call. = FALSE)
  }
  i <- 1L
  if (length(raw) >= 3L &&
      raw[1L] == as.raw(0xef) && raw[2L] == as.raw(0xbb) && raw[3L] == as.raw(0xbf)) {
    i <- 4L
  }
  if (i <= length(raw) && raw[i] == as.raw(0x3c)) {
    stop(
      "Download is not a zip file - content starts with '<' (likely HTML).\n",
      "  URL: ", url_for_message,
      call. = FALSE
    )
  }
  pk <- length(raw) >= 4L && raw[1L] == as.raw(0x50) && raw[2L] == as.raw(0x4b)
  if (!pk) {
    stop(
      "Download is not a valid .zip (missing PK header).\n  URL: ",
      url_for_message,
      call. = FALSE
    )
  }
  invisible(path)
}

.shg_download_failure_message <- function(url, err, has_auth_token) {
  base <- conditionMessage(err)
  bl <- paste(base, collapse = " ")
  lines <- c(
    paste0("Failed to download parameter bundle from:\n  ", url),
    "",
    paste0("Details: ", base)
  )
  hints <- character()
  if (grepl("404|not found", bl, ignore.case = TRUE))
    hints <- c(hints, "- HTTP 404: verify the file URL.")
  if (grepl("401|unauthorized", bl, ignore.case = TRUE))
    hints <- c(hints, "- HTTP 401: set GITHUB_PAT for private GitHub assets.")
  if (length(hints))
    lines <- c(lines, "", "Hints:", hints)
  paste(lines, collapse = "\n")
}

.shg_download_with_httr2 <- function(url, dest_path, auth_hdr) {
  opts <- .shg_download_options()
  req <- httr2::request(url)
  if (length(auth_hdr))
    req <- httr2::req_headers(req, !!!auth_hdr)
  req <- httr2::req_timeout(req, opts$timeout_sec)
  req <- httr2::req_options(req, connecttimeout = opts$connect_sec)
  httr2::req_perform(req, path = dest_path)
}

.shg_download_with_base <- function(url, dest_path, auth_hdr) {
  opts <- .shg_download_options()
  old <- options(timeout = opts$timeout_sec)
  on.exit(options(old), add = TRUE)
  status <- if (length(auth_hdr)) {
    utils::download.file(url, dest_path, mode = "wb", quiet = TRUE, headers = auth_hdr)
  } else {
    utils::download.file(url, dest_path, mode = "wb", quiet = TRUE)
  }
  if (!identical(status, 0L)) {
    stop(
      "download.file() exited with status ", status,
      ". Install package 'httr2' for clearer HTTPS errors.",
      call. = FALSE
    )
  }
}

.shg_download_and_extract <- function(url, cache_path, token) {
  tmp <- tempfile(fileext = ".zip")
  on.exit(unlink(tmp), add = TRUE)
  message("Downloading parameter set from:\n  ", url)
  pat <- .shg_resolve_token(token, url)
  auth_hdr <- if (!is.null(pat)) c(Authorization = paste("Bearer", pat)) else character()
  has_auth_token <- nzchar(Sys.getenv("GITHUB_PAT", "")) ||
    (!is.null(pat) && nzchar(as.character(pat)))
  if (requireNamespace("httr2", quietly = TRUE)) {
    tryCatch(
      .shg_download_with_httr2(url, tmp, auth_hdr),
      error = function(e) stop(.shg_download_failure_message(url, e, has_auth_token), call. = FALSE)
    )
  } else {
    tryCatch(
      .shg_download_with_base(url, tmp, auth_hdr),
      error = function(e) stop(.shg_download_failure_message(url, e, has_auth_token), call. = FALSE)
    )
  }
  .shg_assert_downloaded_zip(tmp, url)
  dir.create(cache_path, recursive = TRUE)
  message("Extracting to cache...")
  tryCatch(
    utils::unzip(tmp, exdir = cache_path),
    error = function(e) {
      unlink(cache_path, recursive = TRUE)
      stop("Could not extract archive: ", conditionMessage(e), call. = FALSE)
    }
  )
  message("Cached at:\n  ", cache_path)
}

.shg_snapshot_root <- function(cache_path) {
  top <- list.files(cache_path, full.names = TRUE)
  top <- top[!grepl("__MACOSX", top)]
  dirs <- top[file.info(top)$isdir]
  if (length(dirs) == 1) {
    cand <- dirs[[1]]
    if (dir.exists(file.path(cand, "params")) ||
        dir.exists(file.path(cand, "smoking")) ||
        dir.exists(file.path(cand, "mortality")))
      return(cand)
  }
  cache_path
}

.shg_bundle_domain <- function(root) {
  params_dir <- file.path(root, "params")
  if (file.exists(file.path(params_dir, "initiation.csv")))
    return("smoking")
  if (file.exists(file.path(params_dir, "acm.csv")) ||
      file.exists(file.path(params_dir, "ocm-excl-lung-cancer.csv")))
    return("mortality")
  if (dir.exists(file.path(root, "smoking")))
    return("legacy_combined")
  NA_character_
}

.shg_copy_or_link <- function(from, to) {
  dir.create(dirname(to), recursive = TRUE, showWarnings = FALSE)
  if (file.exists(to))
    unlink(to)
  ok <- tryCatch(file.link(from, to), error = function(e) FALSE)
  if (!isTRUE(ok))
    file.copy(from, to, overwrite = TRUE)
}

.shg_materialize_engine_tree <- function(smok_cache, mort_cache, combined_path) {
  smok_root <- .shg_snapshot_root(smok_cache)
  mort_root <- .shg_snapshot_root(mort_cache)
  .shg_reject_legacy_combined_zip(smok_cache)
  .shg_reject_legacy_combined_zip(mort_cache)

  smok_dom <- .shg_bundle_domain(smok_root)
  mort_dom <- .shg_bundle_domain(mort_root)
  if (!identical(smok_dom, "smoking")) {
    stop(
      "Smoking bundle missing params/{initiation,cessation,cpd}.csv under: ",
      smok_root,
      call. = FALSE
    )
  }
  if (!identical(mort_dom, "mortality")) {
    stop(
      "Mortality bundle missing params/{acm,ocm-excl-lung-cancer}.csv under: ",
      mort_root,
      call. = FALSE
    )
  }

  if (dir.exists(combined_path))
    unlink(combined_path, recursive = TRUE)
  smk_out <- file.path(combined_path, "smoking")
  mrt_out <- file.path(combined_path, "mortality")
  dir.create(smk_out, recursive = TRUE)
  dir.create(mrt_out, recursive = TRUE)

  for (f in c("initiation.csv", "cessation.csv", "cpd.csv")) {
    .shg_copy_or_link(file.path(smok_root, "params", f), file.path(smk_out, f))
  }
  for (f in c("acm.csv", "ocm-excl-lung-cancer.csv")) {
    src <- file.path(mort_root, "params", f)
    if (file.exists(src))
      .shg_copy_or_link(src, file.path(mrt_out, f))
  }
  combined_path
}

.shg_ensure_combined_params_cache <- function(smok_src, mort_src, mort_params_type) {
  combined <- .shg_params_combined_cache_path(smok_src, mort_src)
  if (.shg_merged_cache_intact(combined, mort_params_type))
    return(combined)

  smok_cache <- .shg_params_cache_path(smok_src)
  mort_cache <- .shg_params_cache_path(mort_src)
  .shg_ensure_zip_extracted(smok_src, smok_cache)
  .shg_ensure_zip_extracted(mort_src, mort_cache)
  .shg_materialize_engine_tree(smok_cache, mort_cache, combined)
  combined
}

.shg_merged_cache_intact <- function(combined_path, mort_params_type = "acm") {
  if (!dir.exists(combined_path))
    return(FALSE)
  mort_file <- if (mort_params_type == "acm") "acm.csv" else "ocm-excl-lung-cancer.csv"
  file.exists(file.path(combined_path, "smoking", "initiation.csv")) &&
    file.exists(file.path(combined_path, "mortality", mort_file))
}

.shg_apply_params <- function(shg, cache_path, mort_params_type) {
  root <- cache_path
  if (!.shg_merged_cache_intact(root, mort_params_type)) {
    alt <- .shg_snapshot_root(cache_path)
    if (.shg_merged_cache_intact(alt, mort_params_type))
      root <- alt
    else
      stop("Merged parameter tree is incomplete under: ", cache_path, call. = FALSE)
  }

  smk_dir <- file.path(root, "smoking")
  mort_dir <- file.path(root, "mortality")
  required <- c(
    file.path(smk_dir, "initiation.csv"),
    file.path(smk_dir, "cessation.csv"),
    file.path(smk_dir, "cpd.csv")
  )
  missing_f <- required[!file.exists(required)]
  if (length(missing_f)) {
    stop(
      "Parameter bundle is missing expected files:\n",
      paste0("  ", missing_f, collapse = "\n"),
      call. = FALSE
    )
  }

  mort_file <- if (mort_params_type == "acm") {
    file.path(mort_dir, "acm.csv")
  } else {
    file.path(mort_dir, "ocm-excl-lung-cancer.csv")
  }
  if (!file.exists(mort_file)) {
    stop(
      "Mortality file not found: ", mort_file,
      "\nAvailable in mortality/: ",
      paste(list.files(mort_dir), collapse = ", "),
      call. = FALSE
    )
  }

  shg$input_data_folder <- root
  shg$initiation_filename <- "smoking/initiation.csv"
  shg$cessation_filename <- "smoking/cessation.csv"
  shg$cpd_filename <- "smoking/cpd.csv"
  shg$mortality_filename <- if (mort_params_type == "acm") {
    "mortality/acm.csv"
  } else {
    "mortality/ocm-excl-lung-cancer.csv"
  }

  message(
    "Parameter set configured",
    "\n  Path:      ", root,
    "\n  Mortality: ", mort_params_type, " (", basename(mort_file), ")"
  )
  invisible(shg)
}
