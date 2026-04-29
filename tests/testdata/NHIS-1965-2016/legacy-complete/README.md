# legacy-complete (full legacy `.txt` tables)

These five files are the **last committed** wide-table bundle that lived under `inst/extdata/default/` in git as `lbc_smokehist_*.txt`, restored here under the usual CLI names:

| File here | Source in git (`HEAD`) |
|-----------|-------------------------|
| `initiation.txt` | `inst/extdata/default/lbc_smokehist_initiation.txt` |
| `cessation.txt` | `inst/extdata/default/lbc_smokehist_cessation.txt` |
| `cpd.txt` | `inst/extdata/default/lbc_smokehist_cpd.txt` |
| `acm.txt` | `inst/extdata/default/lbc_smokehist_ac_mortality.txt` |
| `ocm-excl-lung-cancer.txt` | `inst/extdata/default/lbc_smokehist_oc_mortality.txt` |

To re-copy from the repository after a reset:

```bash
LCD=tests/testdata/NHIS-1965-2016/legacy-complete
git show HEAD:inst/extdata/default/lbc_smokehist_initiation.txt > "$LCD/initiation.txt"
git show HEAD:inst/extdata/default/lbc_smokehist_cessation.txt > "$LCD/cessation.txt"
git show HEAD:inst/extdata/default/lbc_smokehist_cpd.txt > "$LCD/cpd.txt"
git show HEAD:inst/extdata/default/lbc_smokehist_ac_mortality.txt > "$LCD/acm.txt"
git show HEAD:inst/extdata/default/lbc_smokehist_oc_mortality.txt > "$LCD/ocm-excl-lung-cancer.txt"
```

`Rscript tools/trim-nhis-testdata.R` builds `csv-partial/` and `legacy-partial/` from `csv-complete/` only; it does **not** read this folder. For a trimmed **legacy** tree from these full `.txt` files, use `Rscript tools/trim-default-inputs.R` with this directory as the source (see that script’s header).

This directory is listed in `.Rbuildignore` so the full tables are not shipped on CRAN.
