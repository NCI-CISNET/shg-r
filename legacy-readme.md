# Legacy mode

In certain settings you might want to configure the generator using using a configuration input file (rather than properties) and send the output to a text file. You can do this with the `LegacyRunWebVersion()` method. Two example input files are included with the package. Note that if you use `LegacyRunWebVersion()` none of the properties of `shg` you may have set in R will be taken into consideration. Only the properties that you set in the configuration input file will be used. Also note that legacy mode runs with a single segment and with no multi-threading.

```r
library(SmokingHistoryGenerator)
shg <- new(SHGInterface)
wd <- getwd()
# R recommends using the temp folder for external data inputs and outputs
setwd(tempdir())
inputs_folder <- system.file("inputs", package = "SmokingHistoryGenerator")
# Here we use the inputs provided in the package, but you can copy your own input files
file.copy(from = inputs_folder, to = "./", recursive = TRUE)
# Here we use the example configuration files, but you can modify them as needed
shg$LegacyRunWebVersion("./inputs/examples/test_input_example_MersenneTwister.txt")
shg$LegacyRunWebVersion("./inputs/examples/test_input_example_RngStream.txt")
# LegacyRunWebVersion() writes the results to a text file instead of a dataframe
file.edit("MT_test_output.out")
file.edit("RS_test_output.out")
setwd(wd)
```