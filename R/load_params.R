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

#' Load a SHG parameter set from a URL and configure the instance
#'
#' @description
#' Downloads (or reuses a locally cached copy of) a zipped SHG parameter set,
#' unzips it, and sets `input_data_folder` plus relative input filenames on the
#' `SHGInterface` instance (the engine joins folder + filename in C++; do not
#' use absolute paths in the filename fields).
#'
#' The parameter bundle is expected to contain the layout produced by the
#' **shg-params** releases (GitHub or Zenodo):
#' ```
#' <snapshot-id>/
#'   smoking/
#'     initiation.csv
#'     cessation.csv
#'     cpd.csv
#'   mortality/
#'     acm.csv
#'     ocm-excl-lung-cancer.csv
#'   metadata.yml      (optional, informational)
#'   inventory.yml     (optional, informational)
#' ```
#'
#' @section URL forms:
#' Exactly one of `url` or `base_url` must be supplied:
#'
#' | Arguments | Resolved URL |
#' |-----------|--------------|
#' | `url = "https://.../snap.zip"` | used as-is |
#' | `base_url = ".../files", snapshot = "snap-id"` | `.../files/snap-id.zip` |
#' | `base_url = ".../download", path = "v1/snap.zip"` | `.../download/v1/snap.zip` |
#'
#' @param shg An `SHGInterface` instance.  When called as a method
#'   (`shg$load_params(...)`) this argument is bound automatically.
#' @param url Full URL to the `.zip` parameter bundle.
#' @param base_url Base URL; combined with `snapshot` or `path`.
#' @param snapshot Snapshot identifier appended to `base_url` as
#'   `<base_url>/<snapshot>.zip`.
#' @param path Explicit path appended to `base_url`.
#' @param mortality `"acm"` (all-cause mortality, **default**) or `"ocm"`
#'   (other-cause mortality excluding lung cancer).  Controls which file in
#'   `mortality/` is assigned to `shg$mortality_filename`.
#'
#' For private GitHub-hosted zips, set the \code{GITHUB_PAT} environment variable
#' before downloading (used automatically when needed).
#'
#' @section Download timeouts and errors:
#' When the optional package \pkg{httr2} is installed (recommended), HTTPS
#' downloads use a **total transfer timeout** and a **connection timeout**,
#' configurable via R options:
#' \describe{
#'   \item{\code{shg.params.download.timeout_sec}}{Maximum seconds for the whole
#'     transfer (default \code{600}). Large bundles on slow links may need more.}
#'   \item{\code{shg.params.download.connect_sec}}{Maximum seconds to establish
#'     the connection (default \code{60}).}
#' }
#' Without \pkg{httr2}, \code{\link[utils]{download.file}} is used with
#' \code{options(timeout)} set to the same total timeout value (less detailed
#' HTTP error reporting).
#'
#' Failed downloads and bad URLs are reported with the underlying message plus
#' short hints for common cases (HTTP 404/401/403, timeouts, DNS, TLS).
#' Non-zip responses (e.g.\ HTML login or error pages) are detected **before**
#' unpacking so you see a clear message instead of a cryptic unzip failure.
#'
#' @return The `SHGInterface` instance, invisibly (allows chaining).
#'
#' @examples
#' \dontrun{
#' library(SmokingHistoryGenerator)
#' shg <- new(SHGInterface)
#'
#' # Full URL (GitHub Releases)
#' shg$load_params(
#'   url = "https://github.com/NCI-CISNET/shg-params/releases/download/snapshot-id/snapshot-id.zip"
#' )
#'
#' # base_url + snapshot (Zenodo)
#' shg$load_params(
#'   base_url = "https://zenodo.org/records/xxxx/files",
#'   snapshot  = "usa-national@smok-2018-mort-2016"
#' )
#'
#' # base_url + path (GitHub Releases alternative form)
#' shg$load_params(
#'   base_url = "https://github.com/NCI-CISNET/shg-params/releases/download",
#'   path     = "snapshot-id/snapshot-id.zip"
#' )
#'
#' # Switch to other-cause mortality
#' shg$load_params(url = "https://.../snapshot-id.zip", mortality = "ocm")
#'
#' # Private repo: set GITHUB_PAT in the environment before calling load_params()
#'
#' # Local zip by absolute path (no download; extracted to cache on first call)
#' shg$load_params(url = "/path/to/usa-national@smok-2018-mort-2016.zip")
#'
#' # Re-download/re-extract after clearing the cache
#' shg_clear_params_cache()
#' shg$load_params(url = "https://.../snapshot-id.zip")
#'
#' }
#'
#' @seealso [shg_clear_params_cache()], [shg_params_cache_dir()],
#'   [shg_config_bundle()], [shg_load_config()], [shg_use_config_bundle()]
#' @export
shg_load_params <- function(shg,
                             url       = NULL,
                             base_url  = NULL,
                             snapshot  = NULL,
                             path      = NULL,
                             mortality = c("acm", "ocm")) {
  mortality    <- match.arg(mortality)
  resolved_url <- .shg_resolve_params_url(url, base_url, snapshot, path)
  cache_path   <- .shg_params_cache_path(resolved_url)

  if (!dir.exists(cache_path)) {
    if (.shg_is_local_path(resolved_url)) {
      .shg_extract_local(resolved_url, cache_path)
    } else {
      .shg_download_and_extract(resolved_url, cache_path, NULL)
    }
  } else {
    message("Using cached parameter set:\n  ", cache_path)
  }

  .shg_apply_params(shg, cache_path, mortality)
  shg$params_bundle_source <- as.character(resolved_url)
  shg$params_mortality <- as.character(mortality)
  invisible(shg)
}


#' Return the directory where downloaded parameter sets are cached
#'
#' Same path as the read-only \code{SHGInterface} field \code{params_cache_dir}.
#'
#' @return Character scalar: path to the cache directory (may not yet exist).
#' @seealso \code{\link{shg_clear_params_cache}} to remove the entire cache from R.
#' @export
shg_params_cache_dir <- function() {
  tools::R_user_dir("SmokingHistoryGenerator", "cache")
}


#' Clear the SHG parameter cache
#'
#' Deletes the entire directory returned by \code{\link{shg_params_cache_dir}}
#' (all extracted parameter bundles). The next \code{load_params()} will
#' download or extract again as needed.
#'
#' To clear the cache without this function, delete that directory yourself;
#' \code{shg$params_cache_dir} is a read-only property with the same path as
#' \code{shg_params_cache_dir()}.
#'
#' @return Invisibly returns the top-level cache directory path that was removed
#'   (character scalar), or an empty character vector if the cache did not exist.
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

.shg_resolve_params_url <- function(url, base_url, snapshot, path) {
  if (!is.null(url)) {
    if (!is.null(base_url) || !is.null(snapshot) || !is.null(path))
      warning("'url' supplied -- 'base_url', 'snapshot', and 'path' are ignored.")
    return(url)
  }
  if (is.null(base_url))
    stop("Provide either 'url', or 'base_url' with 'snapshot' or 'path'.")
  base_url <- sub("/$", "", base_url)
  if (!is.null(path))
    return(paste0(base_url, "/", sub("^/", "", path)))
  if (!is.null(snapshot))
    return(paste0(base_url, "/", snapshot, ".zip"))
  stop("When 'base_url' is supplied you must also supply 'snapshot' or 'path'.")
}

# Build a filesystem-safe cache key from the URL.
# Uses the last path segment (human readable) plus an 8-char MD5 prefix of
# the full URL to guarantee uniqueness without extra dependencies.
.shg_url_cache_key <- function(url) {
  seg <- sub("\\.zip$", "", basename(url))
  seg <- gsub("[^A-Za-z0-9._@-]", "_", seg)
  tf  <- tempfile()
  on.exit(unlink(tf), add = TRUE)
  writeLines(url, tf)
  h <- substring(tools::md5sum(tf)[[1]], 1, 8)
  paste0(seg, "_", h)
}

.shg_params_cache_path <- function(url) {
  file.path(shg_params_cache_dir(), .shg_url_cache_key(url))
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

.shg_extract_local <- function(zip_path, cache_path) {
  if (!file.exists(zip_path))
    stop("Local parameter zip not found: ", zip_path)
  dir.create(cache_path, recursive = TRUE)
  message("Extracting local parameter set:\n  ", zip_path)
  .shg_assert_downloaded_zip(zip_path, zip_path)
  tryCatch(
    utils::unzip(zip_path, exdir = cache_path),
    error = function(e) {
      unlink(cache_path, recursive = TRUE)
      stop("Extraction failed for ", zip_path, ": ", conditionMessage(e))
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
      "Download is not a zip file - content starts with '<' (likely an HTML error ",
      "or login page). Use a direct \".zip\" download URL; for authenticated hosts ",
      "see the package documentation (e.g. GITHUB_PAT).\n",
      "  URL: ", url_for_message,
      call. = FALSE
    )
  }
  if (length(raw) >= 2L && raw[1L] == as.raw(0x1f) && raw[2L] == as.raw(0x8b)) {
    stop(
      "Download appears to be gzip-compressed, not a .zip file.\n",
      "  URL: ", url_for_message,
      call. = FALSE
    )
  }
  pk <- length(raw) >= 4L && raw[1L] == as.raw(0x50) && raw[2L] == as.raw(0x4b)
  if (!pk) {
    stop(
      "Download is not a valid .zip (missing PK header). The server may have ",
      "returned text or another format.\n",
      "  URL: ", url_for_message,
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
    hints <- c(hints, "- HTTP 404 / not found: verify the file URL (typos, moved releases, or wrong Zenodo/GitHub path).")
  if (grepl("401|unauthorized", bl, ignore.case = TRUE))
    hints <- c(hints, "- HTTP 401: authentication required (set GITHUB_PAT for private GitHub assets).")
  if (grepl("403|forbidden", bl, ignore.case = TRUE))
    hints <- c(hints, "- HTTP 403: access forbidden (token scope, rate limit, or private resource).")
  if (grepl("timed out|timeout|time out|Timeout was reached", bl, ignore.case = TRUE))
    hints <- c(hints, paste0(
      "- Timeout: increase options(shg.params.download.timeout_sec = ...) ",
      "(total seconds; default 600)."
    ))
  if (grepl("Could not resolve host|couldn't resolve", bl, ignore.case = TRUE))
    hints <- c(hints, "- DNS failure: check network connectivity and the hostname.")
  if (grepl("Connection refused|Failed to connect|ECONNREFUSED", bl, ignore.case = TRUE))
    hints <- c(hints, "- Connection refused: server down, wrong port, or firewall.")
  if (grepl("SSL|certificate|TLS|certificate verify failed", bl, ignore.case = TRUE))
    hints <- c(hints, "- TLS/certificate issue (proxy or outdated CA store).")
  if (length(hints))
    lines <- c(lines, "", "Hints:", hints)
  if (!has_auth_token && grepl("github\\.com", url, ignore.case = TRUE))
    lines <- c(lines, "", "Note: For private GitHub releases set GITHUB_PAT.")
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
      ". Install package 'httr2' for clearer HTTPS errors and timeouts.",
      call. = FALSE
    )
  }
}

.shg_download_and_extract <- function(url, cache_path, token) {
  tmp <- tempfile(fileext = ".zip")
  on.exit(unlink(tmp), add = TRUE)
  opts <- .shg_download_options()
  message("Downloading parameter set from:\n  ", url)
  if (requireNamespace("httr2", quietly = TRUE)) {
    message(
      "(timeouts: ", opts$timeout_sec, "s total, ", opts$connect_sec,
      "s connect - see ?shg_load_params)"
    )
  } else {
    message(
      "Note: Install package 'httr2' for HTTPS timeouts (",
      opts$timeout_sec, "s via options(timeout)) and clearer HTTP errors."
    )
  }

  pat <- .shg_resolve_token(token, url)
  auth_hdr <- if (!is.null(pat)) c(Authorization = paste("Bearer", pat)) else character()
  has_auth_token <- nzchar(Sys.getenv("GITHUB_PAT", "")) ||
    (!is.null(pat) && nzchar(as.character(pat)))

  if (requireNamespace("httr2", quietly = TRUE)) {
    tryCatch(
      .shg_download_with_httr2(url, tmp, auth_hdr),
      error = function(e) {
        stop(.shg_download_failure_message(url, e, has_auth_token), call. = FALSE)
      }
    )
  } else {
    tryCatch(
      .shg_download_with_base(url, tmp, auth_hdr),
      error = function(e) {
        stop(.shg_download_failure_message(url, e, has_auth_token), call. = FALSE)
      }
    )
  }

  info <- file.info(tmp)
  message("Download finished (", info$size, " bytes). Verifying archive...")
  .shg_assert_downloaded_zip(tmp, url)

  dir.create(cache_path, recursive = TRUE)
  message("Extracting to cache...")
  withCallingHandlers(
    tryCatch(
      utils::unzip(tmp, exdir = cache_path),
      error = function(e) {
        unlink(cache_path, recursive = TRUE)
        stop(
          "Could not extract archive after download.\n",
          conditionMessage(e),
          call. = FALSE
        )
      }
    ),
    warning = function(w) {
      msg <- conditionMessage(w)
      if (grepl("error 1 in extracting|cannot open zip file", msg, ignore.case = TRUE)) {
        unlink(cache_path, recursive = TRUE)
        stop(
          "The downloaded file is not a valid zip or is corrupted.\n",
          "If the URL points to a web page (HTML) instead of a binary .zip, fix the link ",
          "or authentication.\n",
          "  URL: ", url, "\n",
          "  unzip: ", msg,
          call. = FALSE
        )
      }
      invokeRestart("muffleWarning")
    }
  )
  message("Cached at:\n  ", cache_path)
}

# After unzip, locate the directory that contains smoking/ and mortality/.
# Handles two common layouts:
#   (a) zip has a single top-level folder  -> <cache>/<folder>/smoking/...
#   (b) zip extracts files at root         -> <cache>/smoking/...
.shg_snapshot_root <- function(cache_path) {
  top <- list.files(cache_path, full.names = TRUE)
  top <- top[!grepl("__MACOSX", top)]   # strip macOS artefact dir

  dirs <- top[file.info(top)$isdir]
  if (length(dirs) == 1) {
    cand <- dirs[[1]]
    if (dir.exists(file.path(cand, "smoking")) ||
        dir.exists(file.path(cand, "mortality")))
      return(cand)
  }
  cache_path
}

.shg_apply_params <- function(shg, cache_path, mortality) {
  root    <- .shg_snapshot_root(cache_path)
  smk_dir <- file.path(root, "smoking")
  mrt_dir <- file.path(root, "mortality")

  required <- c(
    file.path(smk_dir, "initiation.csv"),
    file.path(smk_dir, "cessation.csv"),
    file.path(smk_dir, "cpd.csv")
  )
  missing_f <- required[!file.exists(required)]
  if (length(missing_f))
    stop(
      "Parameter bundle is missing expected files:\n",
      paste0("  ", missing_f, collapse = "\n"),
      "\nExpected layout: smoking/{initiation,cessation,cpd}.csv, ",
      "mortality/{acm,ocm-excl-lung-cancer}.csv",
      "\nIf you loaded from a URL, the download may have been an HTML page or a ",
      "truncated file - verify the link and run shg_clear_params_cache() before retrying.",
      call. = FALSE
    )

  mort_file <- if (mortality == "acm") {
    file.path(mrt_dir, "acm.csv")
  } else {
    file.path(mrt_dir, "ocm-excl-lung-cancer.csv")
  }
  if (!file.exists(mort_file))
    stop("Mortality file not found: ", mort_file,
         "\nAvailable in mortality/: ",
         paste(list.files(mrt_dir), collapse = ", "))

  # SHGInterface always joins input_data_folder + filename via AssignFilename()
  # in the C++ layer; filename fields must be relative to that folder, not absolute.
  shg$input_data_folder   <- root
  shg$initiation_filename <- "smoking/initiation.csv"
  shg$cessation_filename  <- "smoking/cessation.csv"
  shg$cpd_filename        <- "smoking/cpd.csv"
  shg$mortality_filename  <- if (mortality == "acm") {
    "mortality/acm.csv"
  } else {
    "mortality/ocm-excl-lung-cancer.csv"
  }

  message(
    "Parameter set configured",
    "\n  Path:      ", root,
    "\n  Mortality: ", mortality, " (", basename(mort_file), ")"
  )
  invisible(shg)
}
