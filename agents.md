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

## Shared Files with shg-cli

The following `src/` files **MUST match shg-cli exactly**:
- `main.cpp`
- `mersenne_class.cpp`, `mersenne_class.h`
- `rng_strategy.h`
- `RngStream.cpp`, `RngStream.h`
- `sim_exception.cpp`, `sim_exception.h`
- `smoking_sim.cpp`, `smoking_sim.h`
- `version.h`

**R-only glue (not synced from CLI):** `wrapper.cpp`, `wrapper.h`, `RcppExports.cpp`

**Bundled inputs (not synced; CRAN-sized subsets):** **`inst/extdata/2018/{smoking,mortality}/*.csv`** (NHIS-1965-2018 csv-partial; cohort columns 1940/1950/2010 in the trimmed bundle) is the **default** (`system.file("extdata", "2018", ...)` + relative `smoking/*.csv`, `mortality/*.csv`). Refresh from `tests/testdata/NHIS-1965-2018/csv-complete/` via **`Rscript tools/refresh-nhis-2018-csv-partial.R`**. **`inst/extdata/2016/`** is **transitional**—remove it (and `find_default_data_path()`'s 2016 fallbacks in `wrapper.h`) when you drop 2016-only fixtures. Older docs also reference **`tools/trim-nhis-testdata.R`** (from `csv-complete/`) when that script exists in the repo. For trimming wide legacy `.txt` in a custom folder, use **`tools/trim-default-inputs.R`** with that directory as the sole argument. After changing bundled inputs or CPD loading, refresh legacy XML fixtures with **`tools/refresh-legacy-fixtures.R`**. Sample Legacy web configs: `tests/testdata/legacy-web-examples/`. Full tables: Zenodo (see `README.md`).

**DO NOT modify shared files in shg-r** without first updating shg-cli. The CLI is the source of truth for shared simulation code.

## Sync Script

Use `tools/shg-sync.py` to manage synchronization:

```bash
python tools/shg-sync.py check              # Check if files match
python tools/shg-sync.py sync-from-cli     # Copy CLI → shg-r (standard)
python tools/shg-sync.py sync-to-cli       # Copy shg-r → CLI (dev only!)
python tools/shg-sync.py update-description  # Refresh src/shg-cli-info.txt from shg-cli
python tools/shg-sync.py validate          # Pre-release validation
```

## Version Management

Two separate version numbers:
- **R package version:** `DESCRIPTION` → `Version` (e.g., 0.0.3)
- **Core engine version:** `src/version.h` → `SHG_CORE_VERSION`

CLI sync state is recorded under a top-level **`shg-cli:`** map in **`src/shg-cli-info.txt`** (YAML). The file is listed in **`.Rbuildignore`** so it is omitted from CRAN source tarballs (and does not trigger the `src/` non-source-file check on submissions built from that tarball). YAML keys are **`MostRecentTag`**, **`CommitHash`**, and **`SrcHash`** (MD5 of shared engine files). R merges these into the object returned by **`packageDescription()`** as **`SHGMostRecentTag`**, **`SHGCommitHash`**, and **`SHGsrcHash`** when the file exists on disk (for example a git checkout with **`devtools::load_all()`**); **`DESCRIPTION`** itself stays CRAN-clean.

Run `python tools/shg-sync.py update-description` to refresh `src/shg-cli-info.txt` from the sibling **shg-cli** checkout.

### When to Bump Versions

| Change Type | DESCRIPTION Version | version.h |
|-------------|---------------------|-----------|
| Wrapper-only change | Bump | No change |
| CLI sync (shared code) | Bump | Update to match CLI |
| New R features | Bump | Depends |

## Release Checklist

1. Run `python tools/shg-sync.py validate` - ensure all checks pass
2. Update `src/version.h` to match CLI (if syncing)
3. Update `DESCRIPTION` and shg-cli sync files:
   - Bump `Version` field in `DESCRIPTION` when the wrapper segment changes
   - Run `python tools/shg-sync.py update-description`
4. Run `R CMD check` (or `rcmdcheck::rcmdcheck(error_on = "warning")` to match **GitHub Actions**, which fails on any WARNING, not only errors)
5. Create PR, wait for CI
6. Merge to master
7. Create git tag
8. Create GitHub release noting CLI version compatibility
