Sys.setenv(PKG_BUILD_EXTRA_FLAGS = "false")
#devtools::clean_dll()
#pkgbuild::compile_dll(path = ".", debug = TRUE)
#devtools::load_all()
devtools::load_all()
library(SmokingHistoryGenerator)
shg <- new(SHGInterface)

#shg$LegacyRunWebVersion("./inst/inputs/examples/test_input_example_MersenneTwister.txt")
shg$LegacyRunWebVersion("./inst/inputs/examples/test_input_example_RngStream.txt")

# N <- 100
# start_time <- Sys.time()
# shg$number_of_segments <- 10
# shg$num_threads <- 1
# shg$rng_strategy <- "RngStream"
# #shg$rng_strategy <- "MersenneTwister"
# RNGSTREAM_SIM <- shg$runSimFromFixedValues(N, 0, 0, 1940)
# end_time <- Sys.time()
# print(end_time - start_time)