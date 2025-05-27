# Custom input datasets

The Smoking History Generator requires a calibrated dataset to run. The dataset includes probability tables for initiation, cessation, mortality (all or other), and cigarettes per day.
A default dataset is included with the `SmokingHistoryGenerator` package for testing and to help users get started quickly. After the package has been installed, the default dataset will be available in a `inputs` directory inside the SmokingHistoryGenerator package folder. You can locate the default inputs directory by running
```r
inputs_dir <- system.file("inputs/default", package="SmokingHistoryGenerator")
#inputs_dir will be /path/to/R/package/SmokingHistoryGenerator/inputs/default
```
The SHGInterface module's `input_data_folder` points to that folder by default. So if you just want to use the default dataset, you shouldn't need to update the `input_data_folder`.

There are also some sample configuration files in the parent `inputs` folder.

Note: if you wish to use the LegacyRunWebVersion() (see below) you will need to update the paths to the probability files
```
INIT_PROB=./path/to/default/lbc_smokehist_initiation.txt
CESS_PROB=./path/to/default/lbc_smokehist_cessation.txt
OCD_PROB=./path/to/default/lbc_smokehist_oc_mortality.txt
CPD_DATA=./path/to/default/lbc_smokehist_cpd.txt
```
```r
MT_config_file <- system.file("./inst/inputs/examples/test_input_example_MersenneTwister.txt", package="SmokingHistoryGenerator")
#inputs_dir will be /path/to/R/package/SmokingHistoryGenerator/inputs/default
```

```
MT_config_file <- system.file("inputs/examples/test_input_example_MersenneTwister.txt", package="SmokingHistoryGenerator")
file.edit(MT_config_file)
# this will open the sample config file. You can then replace the `./inst/inputs/default/` with the value for `inputs_dir`

```