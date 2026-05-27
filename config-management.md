# Configuration management

The SHG package can capture **engine settings**, **parameter-bundle provenance** (zip URL or local path + mortality choice), and **fixed-run parameters** so simulations are reproducible and portable across machines.

## Portable YAML workflow (recommended)

This workflow is designed for scientific reproducibility: the YAML captures the
parameter bundle provenance plus effective run-defining settings so runs can be
recreated across machines/platforms. With the same package/core version and
the same portable YAML inputs, results are expected to match exactly.

1. Load parameters from a released zip (`shg_load_params()` / `shg$load_params()`).
2. Run at least one fixed cohort simulation with `runSimFromFixedValues()` so `individuals`/race/sex/cohort year are recorded on the instance.
3. Save a portable YAML file with `shg$save_config(path)` (recommended). The functional form is `shg_save_config(shg, path)`.
4. Later, load with `shg_load_config()` (or `shg$load_config(path)`), then run with the returned list using `shg_run()` or `shg$runSim(config)`.

**Important:** Portable save is only valid when the **last completed** simulation was `runSimFromFixedValues`. If you run `runSimFromDataFrame()` (a population run) afterward, you must run `runSimFromFixedValues()` again before saving; otherwise `save_config` errors. Fixed-run fields (`individuals` as preferred key; `N` and `repeat` still accepted for backward compatibility), `race`, `sex`, `cohort_year` and effective engine fields (`number_of_segments`, `num_threads`, `rng_strategy`, `seeds`) come from that fixed cohort run. Check `shg$last_completed_sim_was_fixed_cohort()` if needed.

### Defaults-first apply and YAML snippets

- **`shg_apply_config(shg, config)`** resets the instance to **factory defaults**, then applies the named fields in `config`. When **`smok_params_source`** and **`mort_params_source`** are set, derived table paths are omitted and tables are restored via **`shg_load_params()`**, matching **`shg_load_config()`**. Without bundles, explicit `input_data_folder` / per-table filenames in `config` are kept.
- **Factory defaults** use **ACM** mortality (`acm.csv`), consistent with **`mort_params_type = "acm"`**.
- **`shg_write_config_yaml(config, path)`** strips audit keys and omits redundant input paths when both parameter sources are set.
- **`shg$runSimFromFixedValues(..., attach_run_info = TRUE, original_config = NULL)`** (6-argument form) returns a **bundle**: `results`, `original_config`, `repro_config`, `run_info`. The 4-argument form still returns only the `data.frame`.

## Common workflows

### Workflow A: sparse intent config (small, hand-editable)

```r
library(SmokingHistoryGenerator)

shg <- new(SHGInterface)

# Sparse intent: only set what you care about right now
intent <- list(cohort_year = 1950L)
shg_apply_config(shg, intent)
shg_write_config_yaml(intent, "intent.yml")
```

### Workflow B: reproducible run bundle from one simulation

```r
run_cfg <- list(
  individuals = 1e5L,
  race = 0L,
  sex = 0L,
  cohort_year = 2010L
)
bundle <- shg$runSim(run_cfg)

# Bundle slots:
# bundle$results
# bundle$original_config   # sparse / intent
# bundle$repro_config      # complete replay config
# bundle$run_info          # audit metadata (machine, versions, etc.)
```

### Workflow C: save full replay YAML from bundled repro config

```r
shg_write_config_yaml(bundle$repro_config, "repro.yml")
```

### Workflow D: apply replay config in-memory (no YAML round-trip)

```r
shg2 <- new(SHGInterface)
shg_apply_config(shg2, bundle$repro_config)
sim2 <- shg2$runSim(bundle$repro_config)
sim2 <- sim2$results
```

This is the core mechanism for platform-agnostic reproducibility: save once,
rerun elsewhere, and verify equivalent outputs under the same versioned setup.

```r
library(SmokingHistoryGenerator)

shg <- new(SHGInterface)
shg$load_params(
  smoking_url = "https://example.org/usa-national@smok-NHIS-2022.zip",
  mortality_url = "https://example.org/usa-national@mort-v1.0.0.zip",
  mort_params_type = "acm"
)
shg$rng_strategy <- "RngStream"

shg$runSimFromFixedValues(1e5, 0, 0, 2010)

shg$save_config("my-run.yml")
# shg_save_config(shg, "my-run.yml")  # equivalent

# Restore elsewhere (cache may be empty — zip will be re-fetched or re-extracted as needed)
shg2 <- new(SHGInterface)
config <- shg_load_config(shg2, "my-run.yml")
out <- shg2$runSim(config)
sim <- out$results
# or: out <- shg_run(shg2, config); sim <- out$results
```

`shg_load_config()` returns the parsed config **list** (same object to pass to `runSim`), so you never read the YAML file twice.

Private GitHub-hosted zips: set the `GITHUB_PAT` environment variable before `load_params()` / `load_config` triggers a download (same behavior as `shg_load_params()`).

### What goes in the YAML file?

Portable saves include:

- `config_version`
- `smok_params_source`, `mort_params_source`, `mort_params_type`
- Engine fields: `rng_strategy`, `number_of_segments`, `num_threads`, `seeds`, `immediate_cessation_year`
- Fixed-run fields: `individuals` (or aliases `N` / legacy `repeat`), `race`, `sex`, `cohort_year`

They **omit** derived filesystem paths (`input_data_folder`, table filenames). Those are rebuilt when `shg_load_params()` unpacks the smoking and mortality zips referenced by the provenance fields.

## Low-level API: intent config vs repro config

Use `getConfig()` / `useConfig()` for **intent config** (current applied settings):

```r
config <- shg$getConfig(debug = FALSE)
shg2 <- new(SHGInterface)
shg2$useConfig(config)
```

`getConfig()` includes paths on disk as well as provenance and run metadata (when available). It reports the current configured `number_of_segments` / `num_threads` values exactly as set (including `-1` auto intent values).

Use `getReproConfig()` for **repro config** (effective last-run settings):

```r
shg$runSimFromFixedValues(1e5, 0, 0, 2010)
repro <- shg$getReproConfig(debug = FALSE)
```

`getReproConfig()` returns effective runtime segments/threads from the last completed simulation and errors if no simulation has completed on that instance.

For sharing between collaborators, prefer `shg$save_config()` / `shg_save_config()` so paths are not baked into the file and the output stays portability-focused.

With `debug = TRUE`, extra diagnostics are included (RNG fingerprint, package version, platform, memory, etc.) — these are not written by `save_config`.

## Backward compatibility: `run_multi_threaded`

Older configs may list `run_multi_threaded` instead of `num_threads`. `useConfig()` maps that when `num_threads` is absent: `FALSE` → `num_threads = 1`, `TRUE` → `num_threads = -1` (auto). If both fields are present, `num_threads` wins and `run_multi_threaded` is ignored (with a warning).

## Constructor with config

```r
config <- list(
  rng_strategy = "RngStream",
  number_of_segments = 4,
  num_threads = -1,
  immediate_cessation_year = 0
)
shg <- new(SHGInterface, config = config)
```

Use full lists from `getConfig()` when applying engine-only settings; YAML portability for workflows is described above.
