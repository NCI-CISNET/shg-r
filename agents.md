# Agent Guidelines for shg-r

## Git Workflow

### Pushes
- **ALL PUSHes MUST be proposed to the developer** - Never push directly to the repository without explicit approval
- Always propose pushes and wait for developer confirmation before executing

### Commits
- **Commits must be made one at a time** - Each commit should be proposed separately so the developer can review and approve before proceeding
- Do not batch multiple commits together
- Wait for approval after each commit before making the next one

### Code Quality
- **All changes must pass through the linter immediately** - Run the linter on any modified files before committing
- Fix any linter errors before proceeding with commits
- Do not commit code that fails linting checks

### Documentation (CRAN / roxygen)
- **Every exported function and exported S3/Rcpp method needs a `\value` section** in its `.Rd` file (use roxygen `@return`, which becomes `\value`). State the class/structure of the result and what it means. If there is no useful return value, document side effects explicitly (e.g. “No return value, called for side effects” or what is returned invisibly and why).
- **Rcpp-exported methods:** keep durable `@return` / `@examples` in **`R/shg-rcpp-docs.R`** (not only in `src/wrapper.cpp`), because `devtools::document()` / `compileAttributes()` can regenerate `R/RcppExports.R` from C++ and drop roxygen there.
- After adding or changing exports, run **`roxygen2::roxygenise()`** and confirm **`R CMD check`** does not report “Missing Rd-tags: … \\value”.

### R style (integer literals)
- **Prefer plain numeric literals** in R examples, demos, and tests (e.g. `500`, `0`, `1950`) instead of the `L` integer suffix unless an API strictly requires `integer()` and coercion would be ambiguous.

### C++ / Rcpp rebuild hygiene (avoid stale-binary segfaults)

Incremental compiles plus **LTO** (`-flto` in the toolchain) can leave **`src/*.o` out of sync** with regenerated **`RcppExports.cpp`** or other headers. That mismatch often crashes inside **`CppMethod__invoke`** / **`runSimFromFixedValues`** with a fault near address **`0x1`** (wrong vtable / ABI), not a logic bug in the simulator.

**macOS / Homebrew note:** `R CMD COMPILE` (used when building packages) always pulls in **`~/.R/Makevars`** (or **`~/.R/Makevars-$R_PLATFORM`**) if present; it does **not** honor **`R_MAKEVARS_USER`**. Aggressive flags there (e.g. **`-flto`** without the same on the link line, or **`-march=native`**) can yield a **`.so` that segfaults on `dyn.load`** during install. Remove or relax those flags in your personal Makevars, or temporarily rename that file while installing.

**Do this after changing exported Rcpp methods, `RcppExports.cpp`, `wrapper.cpp`, shared engine sources, or when tests suddenly segfault:**

1. **Clean rebuild:** from the package root, run **`bash tools/rebuild-package.sh`** (runs `rm -f src/*.o`, removes any stray `src/*.so`, then **`R CMD INSTALL --preclean .`**).
2. **Or manually:** `rm -f src/*.o && R CMD INSTALL --preclean .`
3. **From R:** delete `src/*.o` (and `src/*.so` / `src/*.dll` on some platforms)—**`pkgbuild::clean_dll()` does not remove object files**, so **`make` can still skip compile** and leave a broken `.so`. Then **`devtools::install(..., args = "--preclean")`** or **`R CMD INSTALL --preclean .`**. (The **R: Test** VS Code task now does this `unlink` step before install.)
4. **`devtools::load_all(compile = TRUE)`** can mix **debug** (`-g -O0`) with **release** flags from **`~/.R/Makevars`** in some setups—if behavior is odd after a load-all compile, use **`--preclean`** install instead.

CI usually does a clean compile; this issue is mainly **local development**.

5. **`devtools::test()` always loads the package with `load_package = "source"`** (pkgload from the source tree), so a prior **`devtools::install()`** into `.R-lib` does **not** affect which `.so` runs during tests. To test the **installed** build, use **`testthat::test_local(getwd(), load_package = "installed")`** (as in the **R: Test** VS Code task) or run **`R CMD check`**.

## Engine source layout

Core simulation code lives under **`src/`** (`main.cpp`, `smoking_sim.cpp`, RNG modules, etc.). R-specific glue is **`wrapper.cpp`**, **`wrapper.h`**, and **`RcppExports.cpp`**.

**Bundled inputs (CRAN-sized subsets):** **`inst/extdata/2018/{smok,mort}/*.csv`** (NHIS-1965-2018 csv-partial; cohort columns 1940/1950/2010 in the trimmed bundle) is the **default** (`system.file("extdata", "2018", ...)` + relative `smok/*.csv`, `mort/*.csv`). Refresh from `tests/testdata/2018/csv-complete/` via **`Rscript tools/refresh-nhis-2018-csv-partial.R`**. NHIS test fixtures live under **`tests/testdata/2018/`** (`csv-partial/`, `legacy-partial/` in the tarball; `csv-complete/` and `legacy-complete/` git-only). For trimming wide legacy `.txt` in a custom folder, use **`tools/trim-default-inputs.R`** with that directory as the sole argument. After changing bundled inputs or CPD loading, refresh legacy XML fixtures with **`tools/refresh-legacy-fixtures.R`**. Sample Legacy web configs: `tests/testdata/legacy-web-examples/`. Full tables: Zenodo (see `README.md`).

## Version Management

- **Package version:** `DESCRIPTION` → `Version` (semver, e.g. `7.1.0`). This is the only user-facing release number and CRAN version.
- **Engine build id:** `src/version.h` → `SHG_CORE_VERSION` (legacy XML `<VERSION>` tag and `get_shg_core_version()`). Bump when simulation behavior changes; not tied to shg-cli.

Earlier releases used `{CLI engine}-{R wrapper}` (e.g. `6.5.3-1.0.1`); that prefix is retired.

## Release Checklist

1. Run **`./tools/check-docker-asan.sh`** after C++ engine changes (CRAN-faithful gcc-ASAN; requires Docker, ~20 min)
2. Run **`./tools/build-cran-submission.sh`** or **`R CMD check`** (`rcmdcheck::rcmdcheck(error_on = "warning")` matches GitHub Actions)
3. Confirm **windows-2022 (R-devel)** passes in **R CMD check (all OS)**; see `dev-readme.md` § CRAN Windows preflight
4. Create PR, wait for CI
5. Merge to main
6. Create git tag and GitHub release
