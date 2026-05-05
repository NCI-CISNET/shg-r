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
#'   snapshot  = "usa-national@smok-2016"
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
#' shg$load_params(url = "/path/to/usa-national@smok-2016.zip")
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
  tryCatch(
    utils::unzip(zip_path, exdir = cache_path),
    error = function(e) {
      unlink(cache_path, recursive = TRUE)
      stop("Extraction failed for ", zip_path, ": ", conditionMessage(e))
    }
  )
  message("Cached at:\n  ", cache_path)
}

.shg_download_and_extract <- function(url, cache_path, token) {
  tmp <- tempfile(fileext = ".zip")
  on.exit(unlink(tmp), add = TRUE)
  message("Downloading parameter set from:\n  ", url)

  pat       <- .shg_resolve_token(token, url)
  auth_hdr  <- if (!is.null(pat)) c(Authorization = paste("Bearer", pat)) else character()

  # Prefer httr2 when available (cleaner error messages, streaming to disk).
  downloaded <- FALSE
  if (requireNamespace("httr2", quietly = TRUE)) {
    req <- httr2::request(url)
    if (length(auth_hdr))
      req <- httr2::req_headers(req, !!!auth_hdr)
    downloaded <- tryCatch({
      httr2::req_perform(req, path = tmp)
      TRUE
    }, error = function(e) {
      warning("httr2 failed (", conditionMessage(e), "); trying download.file().")
      FALSE
    })
  }

  if (!downloaded) {
    tryCatch(
      utils::download.file(url, tmp, mode = "wb", quiet = FALSE, headers = auth_hdr),
      error = function(e) {
        stop("Download failed from:\n  ", url,
             "\n", conditionMessage(e),
             if (!is.null(pat)) "" else
               "\nFor private GitHub releases set the GITHUB_PAT environment variable.")
      }
    )
  }

  dir.create(cache_path, recursive = TRUE)
  message("Extracting to cache...")
  tryCatch(
    utils::unzip(tmp, exdir = cache_path),
    error = function(e) {
      unlink(cache_path, recursive = TRUE)
      stop("Extraction failed for ", url, ": ", conditionMessage(e))
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
  if (length(dirs) == 1L) {
    cand <- dirs[[1L]]
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
    stop("Parameter bundle is missing expected files:\n",
         paste0("  ", missing_f, collapse = "\n"),
         "\nExpected layout: smoking/{initiation,cessation,cpd}.csv, ",
         "mortality/{acm,ocm-excl-lung-cancer}.csv")

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
