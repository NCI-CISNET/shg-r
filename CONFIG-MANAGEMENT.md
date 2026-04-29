# Configuration Management

The SHG package provides functions to capture and restore configuration settings, making it easy to reproduce simulations and share configurations.

## Saving Configuration

Use `getConfig()` to capture the current configuration of an SHG instance:

```r
library(SmokingHistoryGenerator)
shg <- new(SHGInterface)
shg$rng_strategy <- "RngStream"
shg$number_of_segments <- 4
shg$run_multi_threaded <- TRUE

# Set custom seed for RngStream (6-element seed array)
# This seed generates 4 substreams (one for each stream: initiation, cessation, life table, individual)
shg$rngstream_seed <- c(98765, 43210, 11111, 22222, 33333, 44444)

# Get basic configuration
config <- shg$getConfig(debug = FALSE)

# Get configuration with debug information (includes RNG state, package version, system info)
debug_config <- shg$getConfig(debug = TRUE)

# Save configuration to file for later use
saveRDS(config, "my_shg_config.rds")
```

The configuration object includes:
- `config_version`: Version of the config format (currently "1.0")
- All SHG settings: `rng_strategy`, `number_of_segments`, `run_multi_threaded`, `seeds`, `input_data_folder`, `initiation_filename`, `cessation_filename`, `mortality_filename` (same path as legacy `lifetable_filename`), `cpd_filename`, `immediate_cessation_year`
- `timestamp`: When the configuration was captured
- (If `debug = TRUE`) Additional debug information: RNG state fingerprint, package version, system information, memory usage

## Restoring Configuration

Use `useConfig()` to apply a saved configuration to an SHG instance:

```r
# Load saved configuration
config <- readRDS("my_shg_config.rds")

# Apply to a new instance
shg2 <- new(SHGInterface)
shg2$useConfig(config)
# shg2 now has the same configuration as the original instance
```

## Creating Instance with Configuration

You can also create a new SHG instance directly from a configuration:

```r
config <- list(
  config_version = "1.0",
  rng_strategy = "RngStream",
  number_of_segments = 4,
  run_multi_threaded = TRUE,
  seeds = c(98765, 43210, 11111, 22222, 33333, 44444)  # RngStream seed (6 elements)
)

shg <- new(SHGInterface, config = config)
```

## Use Cases

Configuration management is useful for:
- **Reproducibility**: Save the exact configuration used for a simulation
- **Sharing**: Share configurations with collaborators
- **Documentation**: Keep a record of simulation parameters
- **Debugging**: Use `debug = TRUE` to capture system state for troubleshooting

