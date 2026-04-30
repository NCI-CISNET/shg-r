# Custom input datasets

The Smoking History Generator requires a calibrated parameter set to run: probability tables for **initiation**, **cessation**, **mortality** (**ACM** or **OCM**), and **cigarettes per day** (**CPD**).

## Bundled minimal set (CRAN)

The CRAN package ships a **small csv-partial subset** under `inst/extdata/` (installed as `system.file("extdata", package = "SmokingHistoryGenerator")`). Default property values point at these **CSV** files:

| SHG property | Default filename |
| ------------- | ------------- |
| `initiation_filename` | `initiation.csv` |
| `cessation_filename` | `cessation.csv` |
| `mortality_filename` (same as legacy `lifetable_filename`) | `ocm-excl-lung-cancer.csv` |
| `cpd_filename` | `cpd.csv` |

Use **`mortality_filename`** to point at **`acm.csv`** or **`ocm-excl-lung-cancer.csv`**, depending on your analysis. Legacy property **`lifetable_filename`** sets the same path. Wide **`.txt`** tables (CLI / legacy web layout) remain supported when you set filenames and paths accordingly.

After installation, locate the folder with:

```r
inputs_dir <- system.file("extdata", package = "SmokingHistoryGenerator")
```

`SHGInterface` uses that folder by default (`input_data_folder`). Override `input_data_folder` if your files live elsewhere.

## Full NHIS 1965–2016 inputs (Zenodo)

The **full** calibrated NHIS-style tables are **too large for CRAN**. Download the release archive from **Zenodo** (DOI/URL to be published) and unpack so you have a directory such as:

`tests/testdata/NHIS-1965-2016/` (optional; large `csv-complete/` and `legacy-complete/` are excluded from the CRAN tarball via `.Rbuildignore`; `csv-partial/` ships for tests)

Expected files there for a full CLI-style tree mirror the usual names (`initiation.txt`, `cessation.txt`, `cpd.txt`, `acm.txt`, `ocm-excl-lung-cancer.txt`, etc.).

## LegacyRunWebVersion() config keys

Prefer **`MORTALITY_PROB=`** and **`SEED_MORTALITY=`** in text configs. Legacy keys `OCD_PROB=` / `SEED_OCD=` are still accepted by the engine.

Example (paths and extensions must match your files):

```
INIT_PROB=./path/to/initiation.csv
CESS_PROB=./path/to/cessation.csv
MORTALITY_PROB=./path/to/ocm-excl-lung-cancer.csv
CPD_DATA=./path/to/cpd.csv
```

Sample Legacy web configs live under `tests/testdata/legacy-web-examples/` in the **source** tree. Paths in those files assume the **repository root** as the working directory, or replace them with absolute paths built from `system.file("extdata", package = "SmokingHistoryGenerator")`.
