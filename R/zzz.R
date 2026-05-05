# I'm not sure why this needs to be in a file in R folder, but for now I'm leaving it here.

library(Rcpp)
## For R 2.15.1 and later this also works. Note that calling loadModule() triggers
## a load action, so this does not have to be placed in .onLoad() or evalqOnLoad().
loadModule("SmokingSimulator", TRUE)  # Module name

# Attach the R-only load_params method to SHGInterface after the Rcpp module is loaded.
# evalqOnLoad ensures SHGInterface exists before $methods() is called.
# We look up shg_load_params via the package namespace at call time so this works
# correctly under both devtools::load_all() and a fully installed package.
evalqOnLoad({
  SHGInterface$methods(
    load_params = function(url       = NULL,
                           base_url  = NULL,
                           snapshot  = NULL,
                           path      = NULL,
                           mortality = c("acm", "ocm")) {
      get("shg_load_params",
          envir     = asNamespace("SmokingHistoryGenerator"),
          inherits  = FALSE)(
        .self,
        url      = url,
        base_url = base_url,
        snapshot = snapshot,
        path     = path,
        mortality = mortality
      )
    },
    load_config = function(path) {
      get("shg_load_config",
          envir     = asNamespace("SmokingHistoryGenerator"),
          inherits  = FALSE)(
        .self,
        path = path
      )
    },
    save_config = function(path, quiet = FALSE) {
      get("shg_save_config",
          envir     = asNamespace("SmokingHistoryGenerator"),
          inherits  = FALSE)(
        .self,
        path = path,
        quiet = quiet
      )
    },
    repro_config = function(debug = FALSE) {
      .self$getReproConfig(debug)
    },
    runSim = function(config) {
      get(".shg_run_sim",
          envir     = asNamespace("SmokingHistoryGenerator"),
          inherits  = FALSE)(.self, config)
    }
  )
})
