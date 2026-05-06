# SmokingHistoryGenerator

## 6.5.2-1.0.1 (2026-05-06)

### Configuration and defaults

- **`shg_apply_config()`** with **`params_bundle_source`** now calls **`shg_load_params()`** the same way as **`shg_load_config()`** (clears derived paths, restores the bundle). Without a bundle, explicit **`input_data_folder`** / table filenames in the list are still applied.
- Config lists and YAML may use **`mortality`** as an alias for **`params_mortality`**. Normalization uses **`[[` only** so it does not partially match **`mortality_filename`**.
- **`shg_run()`** / **`shg$runSim()`**: if **`repeat`**, **`individuals`**, and **`N`** are all omitted, **`repeat`** defaults to **1000**.
- **Factory / `reset_to_factory_defaults()`** mortality file is **`acm.csv`**, matching **`shg_load_params(..., mortality = "acm")`**.

## 6.5.2-1.0.0 (2026-05-06)

### Configuration

- **`shg_reset_defaults()`** / **`shg$reset_to_factory_defaults()`** restore engine fields to the same defaults as a fresh **`SHGInterface`**.
- **`shg_apply_config(shg, config)`** resets defaults, then applies a sparse or full named list via **`useConfig()`**, so partial YAML/intent configs do not inherit stale instance state.
- **`shg_load_config()`** now starts from factory defaults before applying the YAML bundle (via **`reset_to_factory_defaults()`** in the bundle applier).
- **`shg_write_config_yaml(config, path)`** serializes any config list: drops audit keys, and strips redundant table paths when **`params_bundle_source`** is present (shape-driven “portable” output).

### Simulation return value

- **`shg$runSimFromFixedValues(..., attach_run_info, original_config)`** and **`shg$runSimFromDataFrame(..., attach_run_info, original_config)`** (6-argument forms): when **`attach_run_info`** is **`TRUE`**, the return value is a list with **`results`**, **`original_config`** (sparse intent; default for fixed cohort = repeat/race/sex/cohort_year), **`repro_config`** (post-run **`getReproConfig(FALSE)`**), and **`run_info`** (host/software/audit).
- The 4- and 1-argument forms keep the previous behavior: a **`data.frame`** of simulation output (**`attach_run_info = FALSE`**).
- **`shg_run()`** / **`shg$runSim()`** accept **`attach_run_info`** (default **`TRUE`**; set **`FALSE`** for data-frame-only return).

### Note

Direct **`shg$useConfig()`** without **`shg_apply_config()`** still overlays on the current instance (legacy). Prefer **`shg_apply_config()`** for defaults-first semantics.
