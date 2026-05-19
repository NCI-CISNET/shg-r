# Custom input datasets

The Smoking History Generator requires a calibrated parameter set to run: probability tables for **initiation**, **cessation**, **mortality** (**ACM** or **OCM**), and **cigarettes per day** (**CPD**).

## Bundled minimal set (CRAN)

The CRAN package ships a **default** NHIS-1965–2018 csv-partial under `inst/extdata/2018/` (`smoking/`, `mortality/`): cohort columns **1940, 1950, 2010** (trimmed to race 0 / sex 0). Regenerate from `tests/testdata/NHIS-1965-2018/csv-complete/` with **`Rscript tools/refresh-nhis-2018-csv-partial.R`**. Defaults use `system.file("extdata", "2018", package = "SmokingHistoryGenerator")`. Default property values point at these **CSV** paths relative to `input_data_folder`:

| SHG property | Default filename |
| ------------- | ------------- |
| `initiation_filename` | `smoking/initiation.csv` |
| `cessation_filename` | `smoking/cessation.csv` |
| `mortality_filename` | `mortality/acm.csv` |
| `cpd_filename` | `smoking/cpd.csv` |

You may still point at **flat** filenames (e.g. `initiation.csv` next to `input_data_folder`) for custom layouts. Use `mortality_filename` to select `mortality/acm.csv` or `mortality/ocm-excl-lung-cancer.csv` as needed. Wide `.txt` tables (CLI / legacy web layout) remain supported when you set filenames and paths accordingly.

After installation, locate the folder with:

```r
inputs_dir <- system.file("extdata", "2018", package = "SmokingHistoryGenerator")
```

`SHGInterface` uses that folder by default (`input_data_folder`). Override `input_data_folder` if your files live elsewhere.

## Full NHIS 1965–2018 inputs (`load_params`)

The **full** calibrated NHIS-style tables are **too large for CRAN**.
They are distributed as **parameter bundles** — self-contained `.zip` archives
published on **Zenodo** (preferred, DOI to be announced) and **GitHub Releases**
of [shg-params](https://github.com/NCI-CISNET/shg-params).

Each bundle has this layout:

```
<snapshot-id>/
  smoking/
    initiation.csv
    cessation.csv
    cpd.csv
  mortality/
    acm.csv
    ocm-excl-lung-cancer.csv
  metadata.yml
  inventory.yml
```

### Downloading with `load_params`

`shg$load_params()` downloads the bundle, caches it locally, and sets all
input-file paths on the instance in one call.

#### Zenodo (recommended)

```r
shg$load_params(
  base_url = "https://zenodo.org/records/xxxx/files",
  snapshot  = "usa-national@smok-2018-mort-2016"
)
# resolves to: https://zenodo.org/records/xxxx/files/usa-national@smok-2018-mort-2016.zip
```

#### GitHub Releases — full URL

```r
shg$load_params(
  url = "https://github.com/NCI-CISNET/shg-params/releases/download/usa-national@smok-2018-mort-2016/usa-national@smok-2018-mort-2016.zip"
)
```

#### GitHub Releases — base URL + path

```r
shg$load_params(
  base_url = "https://github.com/NCI-CISNET/shg-params/releases/download",
  path     = "usa-national@smok-2018-mort-2016/usa-national@smok-2018-mort-2016.zip"
)
```

#### Local zip by absolute path

If you already have the bundle on disk, pass the absolute path directly
(no download step; the zip is extracted to the cache on first use):

```r
shg$load_params(url = "/path/to/usa-national@smok-2018-mort-2016.zip")
```

In a **git checkout** of this repo, the same bundle used by tests and demos is at
`tests/testdata/usa-national@smok-2018-mort-2016.zip` (smoking tables from the
2018 NHIS release, mortality from 2016).

### Mortality table: ACM vs OCM

By default `load_params()` selects **all-cause mortality** (`acm.csv`).
Pass `mortality = "ocm"` to use **other-cause mortality** excluding lung cancer
(`ocm-excl-lung-cancer.csv`).  The bundle is not re-downloaded; the call just
re-points `mortality_filename` to the already-cached file.

```r
# All-cause mortality (default)
shg$load_params(base_url = "...", snapshot = "usa-national@smok-2018-mort-2016")

# Other-cause mortality
shg$load_params(base_url = "...", snapshot = "usa-national@smok-2018-mort-2016",
                mortality = "ocm")
```

### Private GitHub repositories

For assets behind a GitHub login, set a Personal Access Token in the environment before downloading:

```r
# e.g. in ~/.Renviron: GITHUB_PAT=ghp_...
Sys.setenv(GITHUB_PAT = "...")  # or rely on ~/.Renviron
shg$load_params(url = "https://github.com/.../snapshot-id.zip")
```

The package uses `httr2` when installed (better error messages and streaming); otherwise it falls back to base-R `download.file()`.

### Cache management

Downloaded bundles are stored in the platform user-cache directory:

```r
shg_params_cache_dir()   # e.g. ~/Library/Caches/org.R-project.R/R/SmokingHistoryGenerator
# Same path on any SHGInterface instance (read-only):
shg$params_cache_dir
```

```r
# Remove the entire parameter cache (all bundles)
shg_clear_params_cache()
```

To clear the cache manually, delete the directory above (or `shg$params_cache_dir`) in the file manager or shell.

After clearing, the next `load_params()` call re-downloads the bundle.

### Parameter shape summary

Use `shg_params_summary()` to inspect the configured parameter-table shape
(cohorts, races, sexes, age ranges) without running a simulation.

```r
shg <- new(SHGInterface)
shg$load_params(url = "/path/to/usa-national@smok-2018-mort-2016.zip")
shg_params_summary(shg)
```

It also works if you manually set `input_data_folder` and the four filenames
(`initiation`, `cessation`, `mortality`, `cpd`).

### Saving config and restoring if the cache was cleared

After `load_params()` and at least one `runSimFromFixedValues()` call (so repeat/race/sex/cohort year are recorded), save a **portable YAML** file with `shg$save_config()` (or `shg_save_config(shg, ...)`). The file is only valid if the **most recent** completed simulation was `runSimFromFixedValues` (a later `runSimFromDataFrame()` / population run means you must run the fixed cohort again before saving). Restore with `shg_load_config()` (alias `shg_use_config_bundle()`). If extracted files are gone but `params_bundle_source` is in the file, that zip URL or path is used to run `load_params()` again automatically.

```r
shg$save_config("my-run.yml")

shg2 <- new(SHGInterface)
config <- shg_load_config(shg2, "my-run.yml")
out <- shg2$runSim(config)
out_df <- out$results
```

See [config-management.md](config-management.md) for the full workflow.

Configs saved **only** from `getConfig()` without loading parameters first do not record `params_bundle_source`; if the cache is cleared you must call `load_params()` yourself with the original URL or path.

## LegacyRunWebVersion() config keys

Prefer `MORTALITY_PROB=` and `SEED_MORTALITY=` in text configs. Legacy keys `OCD_PROB=` / `SEED_OCD=` are still accepted by the engine.

Example (paths and extensions must match your files):

```
INIT_PROB=./path/to/initiation.csv
CESS_PROB=./path/to/cessation.csv
MORTALITY_PROB=./path/to/ocm-excl-lung-cancer.csv
CPD_DATA=./path/to/cpd.csv
```

Sample Legacy web configs live under `tests/testdata/legacy-web-examples/` in the **source** tree. Paths in those files assume the **repository root** as the working directory, or replace them with absolute paths built from `system.file("extdata", "2018", package = "SmokingHistoryGenerator")`.
