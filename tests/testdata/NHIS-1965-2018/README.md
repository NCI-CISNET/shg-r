# NHIS 1965–2018 inputs (local / optional)

Subfolders `csv-complete/` and `legacy-complete/` are **excluded from the CRAN source tarball** (see `.Rbuildignore`) because the full calibrated tables are too large for CRAN. `csv-partial/` and `legacy-partial/` are kept for tests and benchmarks.

## Obtaining the files

1. Download the **Zenodo** release archive for the full NHIS-style SHG inputs (DOI/URL will be published in the main package `README.md`), or run **`Rscript tools/bootstrap-nhis-2018-testdata.R`** when you have the shg-params release trees locally.
2. CSV tables live under `csv-complete/` (`initiation.csv`, `cessation.csv`, `cpd.csv`, `acm.csv`, `ocm-excl-lung-cancer.csv`). Wide legacy `.txt` tables live under `legacy-complete/` with the same basenames as the CLI bundle (`initiation.txt`, `cessation.txt`, `cpd.txt`, `acm.txt`, `ocm-excl-lung-cancer.txt`).

## Using them in R

Point the interface at this folder (only on your machine, after you populate it):

```r
shg <- new(SHGInterface)
shg$input_data_folder <- "/path/to/shg-r/tests/testdata/NHIS-1965-2018/csv-complete"
```

The **trimmed csv-partial** tables that mirror the installed package layout live under `inst/extdata/2018/`. Regenerate them from this tree’s `csv-complete/` with:

```bash
Rscript tools/refresh-nhis-2018-csv-partial.R
```

To trim **wide legacy `.txt`** files you placed under `legacy-complete/`, use `Rscript tools/trim-default-inputs.R <that-directory>` (see that script’s header).
