#!/usr/bin/env Rscript
# Persistent JSON-line worker for pytest (LegacyRunWebVersion).
options(warn = 1)

`%||%` <- function(x, y) if (is.null(x)) y else x

root <- Sys.getenv("SHG_R_ROOT", unset = "")
dev_mode <- nzchar(Sys.getenv("SHG_R_DEV", unset = ""))

if (dev_mode) {
  if (!requireNamespace("pkgload", quietly = TRUE)) {
    stop("pkgload required when SHG_R_DEV=1", call. = FALSE)
  }
  pkgload::load_all(root, quiet = TRUE)
} else {
  library(SmokingHistoryGenerator)
}

if (!requireNamespace("jsonlite", quietly = TRUE)) {
  stop("jsonlite required for pytest R worker", call. = FALSE)
}

shg <- new(SHGInterface)
sink_file <- tempfile("shg_pytest_sink_")

read_request <- function() {
  line <- readLines("stdin", n = 1L, warn = FALSE)
  if (length(line) == 0 || !nzchar(line)) {
    return(NULL)
  }
  jsonlite::fromJSON(line, simplifyVector = FALSE)
}

write_response <- function(resp) {
  cat(jsonlite::toJSON(resp, auto_unbox = TRUE, null = "null"), "\n", sep = "")
  flush(stdout())
}

read_config_paths <- function(input) {
  out <- list(output = "", error = "")
  if (!file.exists(input)) {
    return(out)
  }
  cfg <- readLines(input, warn = FALSE)
  for (line in cfg) {
    if (grepl("^OUTPUTFILE=", line)) {
      out$output <- sub("^OUTPUTFILE=", "", line)
    }
    if (grepl("^ERRORFILE=", line)) {
      out$error <- sub("^ERRORFILE=", "", line)
    }
  }
  out
}

run_legacy <- function(input, cwd) {
  messages <- ""
  err_msg <- ""
  error_file <- ""
  output_file <- ""
  if (nzchar(cwd)) {
    setwd(cwd)
  }
  paths <- read_config_paths(input)
  output_file <- paths$output
  error_file <- paths$error

  unlink(sink_file)
  con <- file(sink_file, open = "wt")
  sink(con)
  sink(con, type = "message")
  r_err <- NULL
  tryCatch(
    shg$LegacyRunWebVersion(input),
    error = function(e) {
      r_err <<- conditionMessage(e)
    }
  )
  sink(type = "message")
  sink()
  close(con)
  if (file.exists(sink_file)) {
    messages <- paste(readLines(sink_file, warn = FALSE), collapse = "\n")
  }

  err_body <- ""
  if (nzchar(error_file) && file.exists(error_file)) {
    err_body <- paste(readLines(error_file, warn = FALSE), collapse = "\n")
  }

  if (!is.null(r_err)) {
    return(list(
      ok = FALSE,
      error = r_err,
      messages = messages,
      error_file = error_file
    ))
  }

  if (grepl("<ERROR>", err_body, fixed = TRUE)) {
    return(list(
      ok = FALSE,
      error = err_body,
      messages = messages,
      error_file = error_file
    ))
  }

  if (!nzchar(output_file) || !file.exists(output_file) || file.info(output_file)$size == 0) {
    return(list(
      ok = FALSE,
      error = if (nzchar(err_body)) err_body else "Missing or empty OUTPUTFILE",
      messages = messages,
      error_file = error_file
    ))
  }

  list(ok = TRUE, error = "", messages = messages, error_file = error_file)
}

while (TRUE) {
  req <- read_request()
  if (is.null(req)) {
    next
  }
  if (identical(req$op, "legacy_run")) {
    resp <- run_legacy(req$input, req$cwd %||% "")
  } else {
    resp <- list(ok = FALSE, error = "unknown op", messages = "", error_file = "")
  }
  write_response(resp)
}
