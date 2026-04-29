# This file is part of the standard setup for testthat.
#
# Debugging failed checks:
# - Full failure output: inspect .../SmokingHistoryGenerator.Rcheck/tests/testthat.Rout.fail
#   (upload that folder as a CI artifact; R CMD check only prints the last ~13 lines to the log).
# - Local: pkgbuild::build(); rcmdcheck::rcmdcheck() or R CMD check ...
# - Richer diffs: SHG_TEST_VERBOSE=1 R CMD check ... (see tests/testthat/setup.R)
#
# Learn more: https://r-pkgs.org/testing-design.html

library(testthat)
library(SmokingHistoryGenerator)

test_check("SmokingHistoryGenerator")
