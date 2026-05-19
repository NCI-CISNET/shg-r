# Developer notes

If you want to contribute to the SHG R wrapper and/or compile the package from source you must first retrieve the `shg-r` repository from Github and open an R session.
```r
setwd("path-to-shg-r")
install.packages("devtools")
Sys.setenv(PKG_BUILD_EXTRA_FLAGS = "false") # optional, but should increase performance
devtools::load_all()
```

For GitHub users/devs who want to (re)install the local checkout into their library,
you can use `pak` directly from the repo root:

```r
install.packages("pak")
setwd("path-to-shg-r")
pak::pak(".")
```

After install, start a **new R session** before reloading/testing to avoid stale package
state from a previously loaded DLL.

Then initially and each time you make changes to the src directory:
```r
# If you want to prevent the pedantic and -O0 optimization flags (slower)
Sys.setenv(PKG_BUILD_EXTRA_FLAGS = "false")

# If you want to force a recompile
devtools::clean_dll()

# Note: debug = TRUE results in "(debug build)" and -O0 -g etc. which overrides Makevars
pkgbuild::compile_dll(path = ".", debug = FALSE)

# Recompile the package if necessary (typically after changes to the C++ source)
devtools::load_all() 
library(SmokingHistoryGenerator)
```

## CRAN Windows preflight

CRAN incoming checks use flavor **`r-devel-windows-x86_64`** (R-devel on 64-bit Windows). You cannot replicate CRAN’s exact farm in GitHub Actions, but you can get close before submitting.

1. **GitHub Actions** — [`.github/workflows/R-CMD-check-all-OS.yaml`](.github/workflows/R-CMD-check-all-OS.yaml) runs **`windows-2022`** with **`r-version: devel`**, `R CMD check --as-cran`, and a CRLF stress step on bundled CSVs under `inst/extdata/` and `tests/testdata/`. Ensure the **`windows-2022 (R-devel)`** job is green on your RC branch before uploading to CRAN.

2. **Local tarball check** — From the package root:

   ```bash
   ./tools/build-cran-submission.sh
   ```

   On Windows, run `R CMD check --as-cran` on the resulting `.tar.gz` if you have a VM or machine available.

3. **win-builder** (closest manual dry run) — Build the same source tarball, then upload it to [win-builder](https://win-builder.r-project.org/) and review the **R-devel / x86_64** result email. This is optional but recommended before a first submission or after engine changes.

Shared engine changes must stay in sync with **shg-cli** (`python tools/shg-sync.py check`).

## CRAN-style `R CMD check` on your machine

- **Reserved `inst/` paths:** Do not ship ad hoc files under `inst/html/` — R uses
  that directory for HTML help. Static assets (e.g. the matrix “rain” viz) live under
  `inst/matrix-rain/` instead.

- **Personal `~/.R/Makevars`:** Flags such as `-march=native`, `-ffast-math`, and
  `-flto` often produce a **NOTE** (“non-portable flag(s)”) and can break `dyn.load`
  for compiled packages. For a clean check, use the template
  **`tools/Makevars.CRAN-safe`** (copy over your user Makevars, or point
  **`R_MAKEVARS_USER`** at that file for one session).

- **HTML manual validation:** If check reports that **HTML Tidy** is missing or too
  old, on macOS with Homebrew install or upgrade [tidy-html5](https://www.html-tidy.org/):

  ```bash
  brew install tidy-html5
  # or, if already installed:
  brew upgrade tidy-html5
  ```

  Ensure `tidy` is on your `PATH` (`which tidy`).

## Performance optimization (developer/local builds)

For **`R CMD check --as-cran`**, prefer **`tools/Makevars.CRAN-safe`** (see above) instead of aggressive flags below.

The package is built with `-O3` by default. For machine-specific speedups on local runs,
you can enable CPU-targeted instructions in `~/.R/Makevars`:

```makefile
CXX17FLAGS += -march=native
```

This can improve throughput for numeric code, but binaries built with `-march=native`
are not portable across different CPU families, and **CRAN’s check will NOTE non-portable flags** if this is enabled during the check.
