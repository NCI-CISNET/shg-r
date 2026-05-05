# NHIS 1965–2016 full inputs (local / optional)

Subfolders `csv-complete/` and `legacy-complete/` are **excluded from the CRAN source tarball** (see `.Rbuildignore`) because the full calibrated tables are too large for CRAN. `csv-partial/` and `legacy-partial/` are kept for tests and benchmarks.

## Obtaining the files

1. Download the **Zenodo** release archive for the full NHIS-style SHG inputs (DOI/URL will be published in the main package `README.md`).
2. Extract the five text tables here with these names (same layout as the CLI `data/NHIS-1965-2016/` bundle):

- `initiation.txt`
- `cessation.txt`
- `cpd.txt`
- `acm.txt` (all-cause mortality)
- `ocm-excl-lung-cancer.txt` (other-cause mortality excluding lung cancer)

## Using them in R

Point the interface at this folder (only on your machine, after you populate it):

```r
shg <- new(SHGInterface)
shg$input_data_folder <- "/path/to/shg-r/tests/testdata/NHIS-1965-2016"
```

The **trimmed csv-partial** tables shipped with the package for CRAN checks live as flat `inst/extdata/*.csv`. Regenerate them from this tree’s `csv-complete/` with:

```bash
Rscript tools/trim-nhis-testdata.R
```

To trim **wide legacy `.txt`** files you placed under `legacy-complete/`, use `Rscript tools/trim-default-inputs.R <that-directory>` (see that script’s header).
