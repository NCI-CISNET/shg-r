# Python pytest suite (R-backed)

Regression tests imported from [shg-cli](https://github.com/NCI-CISNET/shg-cli), executed via
`SHGInterface$LegacyRunWebVersion()` instead of the `lbc_smokehist` executable. Input/output
contracts (legacy `input.txt` → XML `OUTPUTFILE`) are unchanged so golden artifacts and assertions
stay aligned with the CLI suite.

## Overlap with R testthat

[`tests/testthat/test-basic.R`](../testthat/test-basic.R) already compares the same four
regressions, CPD parity checks, and parallel smoke tests that testthat does not cover.

## Skipped CLI tests (not applicable to R)

- Interactive console (`pexpect`) tests
- CLI missing-file stderr test
- `test_r_run_parity.py` (was the inverse bridge to testthat)

Slow parallel matrix and duplicate-analysis tests from shg-cli are not imported yet.

## Environment variables

| Variable | Purpose |
|----------|---------|
| `SHG_R_ROOT` | Repository root (default: inferred from `tests/pytests/`) |
| `SHG_R_DEV=1` | Use `pkgload::load_all()` instead of installed package (local dev) |

CI installs the package with `R CMD INSTALL --preclean .` and does not set `SHG_R_DEV`.

## Local usage

```bash
# After C++ / Rcpp changes
bash tools/rebuild-package.sh

# Or install only
R CMD INSTALL --preclean .

cd tests/pytests
pip install -r requirements-test.txt
pytest

# Dev iteration without reinstall (requires pkgload)
export SHG_R_DEV=1
pytest

# Include @pytest.mark.slow tests (3 extra in test_script.py)
pytest --include-slow
```

CPD all-dot-intensity parity tests append duplicate all-dot placeholder rows at
existing CPD ages (NHIS-2018 has none in the raw tables).

## Regenerating RUN goldens

```bash
bash tools/regenerate-pytest-fixtures-2018.sh
```

Writes to `tests/fixtures/2018/{RS,MT}/` (shared with testthat goldens).

## Regenerating YAML summary goldens

After intentional engine changes affecting summary stats:

```bash
pytest test_script.py::test_simulations --force-regen
```

NHIS CSV tables live under `tests/testdata/2018/csv-complete/` (git-only; required for the fast suite).
Bundled `inst/extdata/2018/` is used as fallback when csv-complete is absent.

Baselines live in `tests/pytests/fixtures/2018/test_simulations_*.yml`.
