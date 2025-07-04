# Custom input datasets

The Smoking History Generator requires a calibrated dataset to run. The dataset includes probability tables for initiation, cessation, mortality (all or other), and cigarettes per day.
A default dataset is included with the `SmokingHistoryGenerator` package for testing and to help users get started quickly. After the package has been installed, the default dataset will be available in a `inputs` directory inside the SmokingHistoryGenerator package folder. You can locate the default inputs directory by running
```r
inputs_dir <- system.file("inputs/default", package="SmokingHistoryGenerator")
# inputs_dir will be /path/to/R/package/SmokingHistoryGenerator/inputs/default
```
The SHGInterface module's `input_data_folder` points to that folder by default. So if you just want to use the default dataset, you shouldn't need to update the `input_data_folder`.

## To use a different input datasets
The easiest way to specify which input files that the SHG should use is to ensure that your input files follow the default filename conventions below and only update the `shg$input_data_folder`. Example:
```
shg$input_data_folder <- '/new/path/to/your/input/files/'
```
Note that the `input_data_folder` is reset to the default when you instantiate a new instance of the `shg` object. So be sure to either re-use the `shg` object or reset the `input_data_folder` accordingly after instantiation.

The default filenames for the inputs are as follows:
|SHG Property   | Default filename|
| ------------- | ------------- |
|initiation_filename | lbc_smokehist_initiation.txt |
|cessation_filename  | lbc_smokehist_cessation.txt |
|lifetable_filename  | lbc_smokehist_oc_mortality.txt |
|cpd_filename        | lbc_smokehist_cpd.txt|

## Changing the paths to data inputs when using LegacyRunWebVersion()
If you wish to use the LegacyRunWebVersion() (see below) you will need to update the paths to the probability files
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
