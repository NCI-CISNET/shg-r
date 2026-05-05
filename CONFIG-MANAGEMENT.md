# Configuration management

The SHG package can capture **engine settings**, **parameter-bundle provenance** (zip URL or local path + mortality choice), and **fixed-run parameters** so simulations are reproducible and portable across machines.

## Portable YAML workflow (recommended)

This workflow is designed for scientific reproducibility: the YAML captures the
parameter bundle provenance plus effective run-defining settings so runs can be
recreated across machines/platforms. With the same package/core version and
the same portable YAML inputs, results are expected to match exactly.

1. Load parameters from a released zip (`shg_load_params()` / `shg$load_params()`).
2. Run at least one fixed cohort simulation with `runSimFromFixedValues()` so repeat/race/sex/cohort year are recorded on the instance.
3. Save a portable YAML file with `shg$save_config(path)` (recommended). The functional form is `shg_save_config(shg, path)`.
4. Later, load with `shg_load_config()` (or `shg$load_config(path)`), then run with the returned list using `shg_run()` or `shg$runSim(config)`.

**Important:** Portable save is only valid when the **last completed** simulation was `runSimFromFixedValues`. If you run `runSimFromDataFrame()` (a population run) afterward, you must run `runSimFromFixedValues()` again before saving; otherwise `save_config` errors. Fixed-run fields (`repeat`, `race`, `sex`, `cohort_year`) and effective engine fields (`number_of_segments`, `num_threads`, `rng_strategy`, `seeds`) come from that fixed cohort run. Check `shg$last_completed_sim_was_fixed_cohort()` if needed.

This is the core mechanism for platform-agnostic reproducibility: save once,
rerun elsewhere, and verify equivalent outputs under the same versioned setup.

```r
library(SmokingHistoryGenerator)

shg <- new(SHGInterface)
shg$load_params(url = "https://example.org/snapshot.zip", mortality = "acm")
shg$rng_strategy <- "RngStream"

shg$runSimFromFixedValues(1e5, 0, 0, 2010)

shg$save_config("my-run.yml")
# shg_save_config(shg, "my-run.yml")  # equivalent

# Restore elsewhere (cache may be empty — zip will be re-fetched or re-extracted as needed)
shg2 <- new(SHGInterface)
config <- shg_load_config(shg2, "my-run.yml")
out <- shg2$runSim(config)
# or: out <- shg_run(shg2, config)
```

`shg_load_config()` returns the parsed config **list** (same object to pass to `runSim`), so you never read the YAML file twice.

Private GitHub-hosted zips: set the `GITHUB_PAT` environment variable before `load_params()` / `load_config` triggers a download (same behavior as `shg_load_params()`).

### What goes in the YAML file?

Portable saves include:

- `config_version`
- `params_bundle_source`, `params_mortality`
- Engine fields: `rng_strategy`, `number_of_segments`, `num_threads`, `seeds`, `immediate_cessation_year`
- Fixed-run fields: `repeat`, `race`, `sex`, `cohort_year`

They **omit** derived filesystem paths (`input_data_folder`, table filenames). Those are rebuilt when `shg_load_params()` unpacks the zip referenced by `params_bundle_source`.

## Low-level API: `getConfig()` / `useConfig()`

You can still snapshot or restore engine state as an R list:

```r
config <- shg$getConfig(debug = FALSE)
shg2 <- new(SHGInterface)
shg2$useConfig(config)
```

`getConfig()` includes paths on disk as well as provenance and run metadata (when available). For sharing between collaborators, prefer `shg$save_config()` / `shg_save_config()` so paths are not baked into the file.

With `debug = TRUE`, extra diagnostics are included (RNG fingerprint, package version, platform, memory, etc.) — these are not written by `save_config`.

## Backward compatibility: `run_multi_threaded`

Older configs may list `run_multi_threaded` instead of `num_threads`. `useConfig()` maps that when `num_threads` is absent: `FALSE` → `num_threads = 1`, `TRUE` → `num_threads = -1` (auto). If both fields are present, `num_threads` wins and `run_multi_threaded` is ignored (with a warning).

## Constructor with config

```r
config <- list(
  config_version = "1.0",
  rng_strategy = "RngStream",
  number_of_segments = 4,
  num_threads = -1,
  immediate_cessation_year = 0L
)
shg <- new(SHGInterface, config = config)
```

Use full lists from `getConfig()` when applying engine-only settings; YAML portability for workflows is described above.
