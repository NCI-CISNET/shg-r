# SmokingHistoryGenerator

## 6.5.2-1.0.0 (2026-05-11)

### Bundled inputs and YAML

- **inst/extdata** layout: smoking tables under **`smoking/`**, mortality under **`mortality/`**; factory defaults use relative paths (`smoking/initiation.csv`, …).
- Portable / written YAML groups **`params_bundle_source`**, **`params_mortality`**, and optional folder paths under a **`params:`** map; **`shg_load_config`** / **`shg_apply_config`** accept nested or flat keys.

### Reproducibility

- **`getReproConfig()`** / portable YAML omit **`num_threads`** (effective segment count and seeds define the run; thread count defaults to auto on reload).
- **`getReproConfig()`** exports **`package_repro`** as **`r_package_version`** only (full install metadata remains internal for fingerprint checks).
- Run bundles with **`attach_run_info = TRUE`** enrich **`repro_config`** with a nested **`results`** block (**`content_md5`**, compact **`summary`**) and a single **`repro_digest`** (engine settings plus R session). Legacy flat **`results_*`** / **`repro_engine_md5`** / **`r_session_md5`** keys in YAML are merged or dropped on load.
- Summaries omit sentinel **`-999`**; **`age_at_death`** is split for never vs ever **death-age** stats (**`mean`**, **`sd`**, **`n_obs`**). Top-level **`ever_smokers`** holds **`count`**, **`fraction`**, and integer **`cpd_mode`** (most common rounded CPD among ever smokers). Keys **`count`** (never/ever totals) and **`n_obs`** (rows contributing to each mean/sd) replace bare **`n`** (YAML 1.1 reserves **`n`**). Initiation and cessation means use **ever smokers only**; **`age_at_death$ever_smokers`** is only death-age statistics (not the same list as top-level **`ever_smokers`**).
- **`shg_save_config(..., results = )`** optionally writes those verification fields (including **`content_md5`** and compact results summary metadata when **`results`** is supplied).

### Configuration

- **`shg_load_params()`** (URLs): **`shg.params.download.timeout_sec`** (default 600) and **`shg.params.download.connect_sec`** (default 60) when **`httr2`** is installed; clearer HTTP/network errors; HTML and non-zip responses detected before unzip.

- **`shg_reset_defaults()`** / **`shg$reset_to_factory_defaults()`** restore engine fields to the same defaults as a fresh **`SHGInterface`**.
- **`shg_apply_config(shg, config)`** resets defaults, then applies a sparse or full named list via **`useConfig()`**, so partial YAML/intent configs do not inherit stale instance state.
- **`shg_apply_config()`** with **`params_bundle_source`** now calls **`shg_load_params()`** the same way as **`shg_load_config()`** (clears derived paths, restores the bundle). Without a bundle, explicit **`input_data_folder`** / table filenames in the list are still applied.
- **`shg_load_config()`** now starts from factory defaults before applying the YAML bundle (via **`reset_to_factory_defaults()`** in the bundle applier).
- **`shg_write_config_yaml(config, path)`** serializes any config list: drops audit keys, and strips redundant table paths when **`params_bundle_source`** is present (shape-driven “portable” output).
- Config lists and YAML may use **`mortality`** as an alias for **`params_mortality`**. Normalization uses **`[[` only** so it does not partially match **`mortality_filename`**.
- **Factory / `reset_to_factory_defaults()`** mortality file is **`mortality/acm.csv`**, matching **`shg_load_params(..., mortality = "acm")`** bundle layout.

### Simulation return value

- **`shg$runSimFromFixedValues(..., attach_run_info, original_config)`** and **`shg$runSimFromDataFrame(..., attach_run_info, original_config)`** (6-argument forms): when **`attach_run_info`** is **`TRUE`**, the return value is a list with **`results`**, **`original_config`** (sparse intent; default for fixed cohort = repeat/race/sex/cohort_year), **`repro_config`** (post-run **`getReproConfig(FALSE)`**), and **`run_info`** (host/software/audit).
- The 4- and 1-argument forms keep the previous behavior: a **`data.frame`** of simulation output (**`attach_run_info = FALSE`**).
- **`shg_run()`** / **`shg$runSim()`** accept **`attach_run_info`** (default **`TRUE`**; set **`FALSE`** for data-frame-only return).
- **`shg_run()`** / **`shg$runSim()`**: if **`repeat`**, **`individuals`**, and **`N`** are all omitted, **`repeat`** defaults to **1000**.

### Note

Direct **`shg$useConfig()`** without **`shg_apply_config()`** still overlays on the current instance (legacy). Prefer **`shg_apply_config()`** for defaults-first semantics.
