// The Rcpp Smoking History Generator is a convenience wrapper in R for the Smoking History Generator (SHG) application.
// Copyright (C) 2024, John Clarke

// CISNET (www.cisnet.cancer.gov)
// Rcpp wrapper for Smoking History Generator Application
// Application to Simulate Initiation and Cessation Ages of individuals based on sex, race and year of birth.
// File: wrapper.cpp
// Author: John Clarke
// E-Mail: john.clarke@cornerstonenw.com
// NCI Contact: Natasha Stout

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see https://www.gnu.org/licenses/.

#include <cstdlib>    // General utilities
#include <cstring>    // C-style string functions
#include <iostream>   // Input/output stream objects
#include <filesystem>
#include <fstream>    // File stream objects
#include <string>     // std::string class
#include <limits>     // Numeric limits
#include <cctype>     // Character classification and conversion
#include <cstdio>     // C standard input/output library
#include <ctime>      // Time and date functions
#include <sstream>    // String stream classes
#include <vector>     // std::vector container
#include <iterator>   // Iterator definitions
#include <algorithm>  // Algorithms like sort, find, etc.
#include <future>     // Asynchronous operations
#include <thread>     // Thread support
#include <chrono>     // Timing
#include <climits>
#include <cmath>
#include <cstdint>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "wrapper.h"
#include "smoking_sim.h"
#include "sim_exception.h"
#include "version.h"

namespace {

/** MT19937 init uses 32-bit seeds; mask when passing user-supplied values (may exceed UINT_MAX) to the engine. */
inline unsigned long mt_seed_to_engine_arg(double stored_user_seed) {
   if (!R_FINITE(stored_user_seed) || stored_user_seed < 0.0) {
      Rcpp::stop("Invalid MersenneTwister seed (must be finite and non-negative).");
   }
   const std::uint64_t u = static_cast<std::uint64_t>(std::llround(stored_user_seed));
   return static_cast<unsigned long>(u & 0xffffffffULL);
}

}  // namespace
#include <Rcpp.h>

using namespace std;

// Fast integer to string conversion (10-20x faster than std::to_string)
// Writes digits forward to avoid reverse, returns pointer past end
inline char* fast_itoa(int val, char* buf) {
   if (val < 0) {
      *buf++ = '-';
      if (val == INT_MIN) {
         const char* s = "2147483648";
         while (*s) {
            *buf++ = *s++;
         }
         return buf;
      }
      val = -val;
   }
   // Handle 0-99 directly (most common case for ages and CPD)
   if (val < 10) {
      *buf++ = '0' + val;
   } else if (val < 100) {
      *buf++ = '0' + val / 10;
      *buf++ = '0' + val % 10;
   } else {
      // General case: write digits, then reverse
      char* start = buf;
      do {
         *buf++ = '0' + val % 10;
         val /= 10;
      } while (val > 0);
      std::reverse(start, buf);
   }
   return buf;
}

// Append integer to string using fast conversion
inline void append_int(std::string& s, int val) {
   char buf[16];
   char* end = fast_itoa(val, buf);
   s.append(buf, end - buf);
}

// We need to create a wrapper class rather than reference Smoking_Simulator directly
// because (among other constraints) RCPP does not support classes with constructors
// that take more than 6 arguments

// SHGInterface
//' SHGInterface Class
//' @name SHGInterface
//' @title SHGInterface
//' @aliases SHGInterface
//' @export
//' @description The SHG Interface class provides an Rcpp interface to the Smoking History Generator (SHG)
//' @field number_of_segments Number of segments to use for simulation. Use -1 for auto-calculation (default), 1 for single segment, or N > 1 for explicit segment count. Auto-calculation uses: min(cores * 10, repeat / 1000). Note: MersenneTwister RNG is restricted to 1 segment.
//' @field num_threads Thread count: -1 = auto (all cores, multi-threaded), 1 = single-threaded, N = use N threads. Default: -1. Note: MersenneTwister RNG requires num_threads = 1.
//' @field rng_strategy 'RngStream' for MRG32k3a (default) or 'MersenneTwister' for Mersenne Twister RNG. 'RngStream' is recommended for reproducibility especially with multi-threaded simulations. Note: MersenneTwister RNG is restricted to single-segment, non-parallel execution due to limitations in maintaining IID properties across segments.
//' @field input_data_folder Set or get the base folder for input data files
//' @field initiation_filename Set or get the initiation filename
//' @field cessation_filename Set or get the cessation filename
//' @field mortality_filename Set or get the mortality probabilities filename (e.g. acm.csv or ocm-excl-lung-cancer.csv)
//' @field smok_params_source URL or local path of the last load_params() smoking zip (empty if unset)
//' @field mort_params_source URL or local path of the last load_params() mortality zip (empty if unset)
//' @field mort_params_type Mortality table from last load_params(): acm or ocm (empty if unset)
//' @field params_cache_dir Read-only. Directory where load_params() stores extracted bundles (same as shg_params_cache_dir()). Delete this folder to clear the cache manually.
//' @field cpd_filename Set or get the cpd filename
//' @field immediate_cessation_year Set or get Immediate Cessation Year; If 0, no immediate cessation
//' @field mt_seeds Set or get MersenneTwister seeds. Must be a numeric vector of exactly 4 values (one for each stream: initiation, cessation, life table, individual). If not set, default seeds are used. Only used when rng_strategy is "MersenneTwister".
//' @field rngstream_seed Set or get RngStream seed. Must be a numeric vector of exactly 6 values (a single seed vector that generates 4 substreams, one for each stream: initiation, cessation, life table, individual). If not set, default seed is used. Only used when rng_strategy is "RngStream".

SHGInterface::SHGInterface() {
   // For now there is no initialize needed;
   // The Smoking_Simulators are created on the fly
}

SHGInterface::SHGInterface(Rcpp::List config) {
   // Constructor with optional config parameter
   // The Smoking_Simulators are created on the fly
   if (config.size() > 0) {
      reset_to_factory_defaults();
      useConfig(config);
   }
}

void SHGInterface::reset_to_factory_defaults() {
   input_data_folder = find_default_data_path();
   initiation_filename = R_INITIATION_DATA_FILE;
   cessation_filename = R_CESSATION_DATA_FILE;
   mortality_filename_ = R_ACM_DATA_FILE;
   cpd_filename = R_CPD_DATA_FILE;
   smok_params_source_.clear();
   mort_params_source_.clear();
   mort_params_type_.clear();
   immediate_cessation_year = 0;
   number_of_segments = -1;
   num_threads = -1;
   rng_strategy = "RngStream";
   cpd_format = "sparse";
   output_file.clear();
   mt_seeds.clear();
   rngstream_seed.clear();
   has_effective_runtime_config_ = false;
   last_effective_number_of_segments_ = -1;
   last_effective_num_threads_ = -1;
   has_last_cohort_year_ = false;
   last_cohort_year_ = 0;
   has_last_fixed_run_ = false;
   last_fixed_repeat_ = 0;
   last_fixed_race_ = 0;
   last_fixed_sex_ = 0;
   next_dataframe_call_is_fixed_cohort_ = false;
   last_completed_sim_was_fixed_cohort_ = false;
   last_num_races = 0;
   last_num_sexes = 0;
   last_num_cohorts = 0;
   last_min_init_age = 0;
   last_max_init_age = 0;
   last_min_cess_age = 0;
   last_max_cess_age = 0;
   last_cpd_min_age = 0;
   last_cpd_max_age = 0;
   last_num_intensity_grps = 0;
   last_first_cohort_start = 0;
   last_first_cohort_end = 0;
   last_last_cohort_start = 0;
   last_last_cohort_end = 0;
}

std::string SHGInterface::get_shg_core_version() const {
   return std::string(SHG_CORE_VERSION);
}

Rcpp::RObject SHGInterface::finalizeSimOutput(Rcpp::DataFrame df,
                                              bool attach_run_info,
                                              const Rcpp::List& original_config) {
   if (!attach_run_info) {
      return Rcpp::wrap(df);
   }
   Rcpp::List repro = getReproConfig(false);
   Rcpp::Environment pkg_env = Rcpp::Environment::namespace_env("SmokingHistoryGenerator");
   Rcpp::Function enrich_repro = pkg_env[".shg_enrich_repro_config"];
   repro = Rcpp::as<Rcpp::List>(enrich_repro(repro, df));
   Rcpp::Function build_run_info = pkg_env[".shg_build_run_info"];
   Rcpp::List run_info = build_run_info(Rcpp::Named("core_version") = std::string(SHG_CORE_VERSION));
   return Rcpp::List::create(
      Rcpp::Named("results") = df,
      Rcpp::Named("original_config") = original_config,
      Rcpp::Named("repro_config") = repro,
      Rcpp::Named("run_info") = run_info);
}

std::string SHGInterface::get_params_cache_dir() {
   Rcpp::Environment tools_ns = Rcpp::Environment::namespace_env("tools");
   Rcpp::Function R_user_dir = tools_ns["R_user_dir"];
   return Rcpp::as<std::string>(R_user_dir("SmokingHistoryGenerator", "cache"));
}

void SHGInterface::set_rng_strategy(string strategy) {
   if (strategy != "MersenneTwister" && strategy != "RngStream") {
      Rcpp::stop("Invalid RNG strategy. Must be 'RngStream' or 'MersenneTwister'");
   }
   
   // If switching to MersenneTwister, enforce restrictions (informational: not a warning)
   if (strategy == "MersenneTwister") {
      if (number_of_segments > 1) {
         Rcpp::Function rl_message("message");
         rl_message("Resetting number_of_segments to 1 for MersenneTwister RNG.");
         number_of_segments = 1;
      }
      if (num_threads != 1) {
         Rcpp::Function rl_message("message");
         rl_message("Resetting num_threads to 1 for MersenneTwister RNG (single-threaded only).");
         num_threads = 1;
      }
   }
   
   rng_strategy = strategy;
}

void SHGInterface::set_number_of_segments(int n) {
   if (n < -1 || n == 0) {
      Rcpp::stop("number_of_segments must be -1 (auto) or >= 1");
   }
   
   if (rng_strategy == "MersenneTwister" && n > 1) {
      Rcpp::stop("MersenneTwister RNG cannot maintain IID properties with multiple segments. MersenneTwister is restricted to 1 segment. Use RngStream for multiple segments.");
   }
   
   number_of_segments = n;
}

void SHGInterface::set_num_threads(int n) {
   if (n < -1 || n == 0) {
      Rcpp::stop("num_threads must be -1 (auto), 1 (single-threaded), or > 1");
   }
   
   // MersenneTwister requires single-threaded
   if (rng_strategy == "MersenneTwister" && n != 1) {
      Rcpp::stop("MersenneTwister RNG requires single-threaded execution (num_threads = 1). Use RngStream for multi-threading.");
   }
   
   // Cap to available cores with warning
   int availableCores = std::thread::hardware_concurrency();
   if (availableCores < 1) availableCores = 1;
   if (n > availableCores) {
      Rcpp::Function warning("warning");
      std::string msg = "num_threads=" + std::to_string(n) + " exceeds available cores (" + 
                        std::to_string(availableCores) + "). Using " + std::to_string(availableCores) + 
                        " threads. Using more threads than cores provides no benefit and may cause instability.";
      warning(msg, Rcpp::Named("call.") = false);
      n = availableCores;
   }
   
   // Warn if num_threads > 1 but number_of_segments == 1 (no parallelism possible)
   if (n != 1 && number_of_segments == 1) {
      Rcpp::Function warning("warning");
      warning("num_threads > 1 or -1 (auto) has no effect when number_of_segments is 1. Consider number_of_segments = -1 (auto).", Rcpp::Named("call.") = false);
   }
   
   num_threads = n;
}

void SHGInterface::set_cpd_format(string format) {
   // "legacy" is the old "full" format for backwards compatibility
   if (format != "none" && format != "sparse" && format != "legacy") {
      Rcpp::stop("cpd_format must be 'none', 'sparse', or 'legacy'. Provided: " + format);
   }
   cpd_format = format;
}

Rcpp::NumericVector SHGInterface::get_mt_seeds() {
   Rcpp::NumericVector result(mt_seeds.size());
   for (size_t i = 0; i < mt_seeds.size(); i++) {
      result[i] = mt_seeds[i];
   }
   return result;
}

void SHGInterface::set_mt_seeds(Rcpp::NumericVector seeds) {
   if (seeds.size() != 4) {
      Rcpp::stop("MersenneTwister requires exactly 4 seeds (one for each stream: initiation, cessation, life table, individual). Provided: " + to_string(seeds.size()));
   }
   mt_seeds.clear();
   mt_seeds.reserve(4);
   for (int i = 0; i < 4; i++) {
      if (!R_FINITE(seeds[i])) {
         Rcpp::stop("MersenneTwister seeds must be finite numeric values (no NA/NaN/Inf).");
      }
      mt_seeds.push_back(seeds[i]);
   }
}

Rcpp::NumericVector SHGInterface::get_rngstream_seed() {
   Rcpp::NumericVector result(rngstream_seed.size());
   for (size_t i = 0; i < rngstream_seed.size(); i++) {
      result[i] = static_cast<double>(rngstream_seed[i]);
   }
   return result;
}

void SHGInterface::set_rngstream_seed(Rcpp::NumericVector seed) {
   if (seed.size() != 6) {
      Rcpp::stop("RngStream requires a seed vector with exactly 6 elements) Provided: " + to_string(seed.size()));
   }
   rngstream_seed.clear();
   rngstream_seed.reserve(6);
   for (int i = 0; i < 6; i++) {
      rngstream_seed.push_back(static_cast<unsigned long>(seed[i]));
   }
}

Rcpp::NumericVector SHGInterface::get_current_seeds() {
   if (rng_strategy == "MersenneTwister") {
      if (mt_seeds.size() == 4) {
         return get_mt_seeds();
      }
      return Rcpp::NumericVector::create(1898587603, 1468371936, 1551308340, 1590227640);
   } else if (rng_strategy == "RngStream") {
      if (rngstream_seed.size() == 6) {
         return get_rngstream_seed();
      }
      return Rcpp::NumericVector::create(12345, 12345, 12345, 12345, 12345, 12345);
   } else {
      Rcpp::stop("Invalid RNG strategy. Cannot retrieve seeds for strategy: " + rng_strategy);
   }
}

void SHGInterface::reset_seeds_to_defaults() {
   if (rng_strategy == "MersenneTwister") {
      // Default MT seeds: 1898587603, 1468371936, 1551308340, 1590227640
      Rcpp::NumericVector default_seeds = Rcpp::NumericVector::create(1898587603, 1468371936, 1551308340, 1590227640);
      set_mt_seeds(default_seeds);
   } else if (rng_strategy == "RngStream") {
      // Default RngStream seed: c(12345, 12345, 12345, 12345, 12345, 12345)
      Rcpp::NumericVector default_seed = Rcpp::NumericVector::create(12345, 12345, 12345, 12345, 12345, 12345);
      set_rngstream_seed(default_seed);
   } else {
      Rcpp::stop("Invalid RNG strategy. Cannot reset seeds for strategy: " + rng_strategy);
   }
}

Rcpp::NumericVector SHGInterface::get_rng_state_fingerprint() {
   // Create a temporary simulator with current seeds to get the RNG state fingerprint
   std::unique_ptr<Smoking_Simulator> qSimulator(loadSimulator());
   
   // Set RNG strategy with user-specified seeds or defaults (same logic as runSimSegment)
   if (rng_strategy == "MersenneTwister") {
      if (mt_seeds.size() == 4) {
         qSimulator->setRNGStrategy(new MersenneTwisterRNG(
            mt_seed_to_engine_arg(mt_seeds[0]),
            mt_seed_to_engine_arg(mt_seeds[1]),
            mt_seed_to_engine_arg(mt_seeds[2]),
            mt_seed_to_engine_arg(mt_seeds[3])));
      } else {
         qSimulator->setRNGStrategy(new MersenneTwisterRNG(1898587603, 1468371936, 1551308340, 1590227640));
      }
   }
   else if (rng_strategy == "RngStream") {
      if (rngstream_seed.size() == 6) {
         unsigned long seed_array[6];
         for (int i = 0; i < 6; i++) {
            seed_array[i] = rngstream_seed[i];
         }
         qSimulator->setRNGStrategy(new RngStreamRNG(seed_array));
      } else {
         qSimulator->setRNGStrategy(new RngStreamRNG());
      }
   }
   else {
      Rcpp::stop("Invalid RNG strategy. Cannot get fingerprint for strategy: " + rng_strategy);
   }
   
   // Get the fingerprint from the RNG strategy
   std::vector<double> fingerprint = qSimulator->getRNGStateFingerprint();
   
   // Convert to Rcpp::NumericVector
   Rcpp::NumericVector result(fingerprint.size());
   for (size_t i = 0; i < fingerprint.size(); i++) {
      result[i] = fingerprint[i];
   }
   
   return result;
}

Smoking_Simulator* SHGInterface::loadSimulator()
{
   char *sInitiationFile = AssignFilename(input_data_folder.c_str(), initiation_filename.c_str());
   char *sCessationFile = AssignFilename(input_data_folder.c_str(), cessation_filename.c_str());
   char *sLifeTableFile = AssignFilename(input_data_folder.c_str(), mortality_filename_.c_str()); // OCM or ACM mortality table
   char *sCPDDataFile = AssignFilename(input_data_folder.c_str(), cpd_filename.c_str());
   int wCessationYear = immediate_cessation_year; // 0 is default and specifies no immediate cessation

   if (!fileExists(sInitiationFile)) {
      Rcpp::stop("Input file does not exist: " + string(sInitiationFile));
   }
   if (!fileExists(sCessationFile)) {
      Rcpp::stop("Input file does not exist: " + string(sCessationFile));
   }
   if (!fileExists(sLifeTableFile)) {
      Rcpp::stop("Input file does not exist: " + string(sLifeTableFile));
   }
   if (!fileExists(sCPDDataFile)) {
      Rcpp::stop("Input file does not exist: " + string(sCPDDataFile));
   }

   const char *sCPDIntensityFile = ""; // no longer used, but variable needed for function signature
   short wOutputType = 1; // Not relevant for R but must include because we want to reuse the SHG CLI code. see SetOutputType() in smoking_sim.cpp and enum OutputType in smoking_sim.h

   return new Smoking_Simulator(sInitiationFile, sCessationFile,
                                       sLifeTableFile, sCPDIntensityFile,
                                       sCPDDataFile, wOutputType,
                                       wCessationYear);
};

//' @name get_data_shape
//' @title get_data_shape method
//' @description Returns a list containing information about the shape/dimensions of the current input data files.
//'              It reads the configured parameter files directly and does not require running a simulation first.
//' @return A list with data shape information including races, sexes, cohorts, age ranges, cohort boundaries, and CPD statistics.
Rcpp::List SHGInterface::get_data_shape() {
   string initFile = input_data_folder + "/" + initiation_filename;
   string cessFile = input_data_folder + "/" + cessation_filename;
   string lifeFile = input_data_folder + "/" + mortality_filename_;
   string cpdFile = input_data_folder + "/" + cpd_filename;

   SmokingSimulatorSharedData* pSharedData = Smoking_Simulator::CreateSharedData(
      initFile.c_str(), cessFile.c_str(), lifeFile.c_str(), cpdFile.c_str());

   Rcpp::IntegerVector cohort_starts(pSharedData->gwNumBirthCohorts);
   Rcpp::IntegerVector cohort_ends(pSharedData->gwNumBirthCohorts);
   for (int i = 0; i < pSharedData->gwNumBirthCohorts; i++) {
      cohort_starts[i] = pSharedData->gwYOBCohortStartYrs[i];
      cohort_ends[i] = pSharedData->gwYOBCohortEndYrs[i];
   }

   Rcpp::List out = Rcpp::List::create(
      Rcpp::Named("num_races") = pSharedData->gwNumRaceValues,
      Rcpp::Named("num_sexes") = pSharedData->gwNumSexValues,
      Rcpp::Named("num_cohorts") = pSharedData->gwNumBirthCohorts,
      Rcpp::Named("cohort_start_years") = cohort_starts,
      Rcpp::Named("cohort_end_years") = cohort_ends,
      Rcpp::Named("first_cohort") = Rcpp::IntegerVector::create(
         Rcpp::Named("start") = pSharedData->gwNumBirthCohorts > 0 ? pSharedData->gwYOBCohortStartYrs[0] : NA_INTEGER,
         Rcpp::Named("end") = pSharedData->gwNumBirthCohorts > 0 ? pSharedData->gwYOBCohortEndYrs[0] : NA_INTEGER
      ),
      Rcpp::Named("last_cohort") = Rcpp::IntegerVector::create(
         Rcpp::Named("start") = pSharedData->gwNumBirthCohorts > 0 ? pSharedData->gwYOBCohortStartYrs[pSharedData->gwNumBirthCohorts - 1] : NA_INTEGER,
         Rcpp::Named("end") = pSharedData->gwNumBirthCohorts > 0 ? pSharedData->gwYOBCohortEndYrs[pSharedData->gwNumBirthCohorts - 1] : NA_INTEGER
      ),
      Rcpp::Named("initiation_ages") = Rcpp::IntegerVector::create(
         Rcpp::Named("min") = pSharedData->gwMinInitiationAge,
         Rcpp::Named("max") = pSharedData->gwMaxInitiationAge
      ),
      Rcpp::Named("cessation_ages") = Rcpp::IntegerVector::create(
         Rcpp::Named("min") = pSharedData->gwMinCessationAge,
         Rcpp::Named("max") = pSharedData->gwMaxCessationAge
      ),
      Rcpp::Named("mortality_ages") = Rcpp::IntegerVector::create(
         Rcpp::Named("min") = pSharedData->gwMinMortalityAge,
         Rcpp::Named("max") = pSharedData->gwMaxMortalityAge
      ),
      Rcpp::Named("mortality_years") = Rcpp::IntegerVector::create(
         Rcpp::Named("min") = pSharedData->gwMinMortalityYear,
         Rcpp::Named("max") = pSharedData->gwMaxMortalityYear
      ),
      Rcpp::Named("cpd_ages") = Rcpp::IntegerVector::create(
         Rcpp::Named("min") = (int)pSharedData->gwCpdMinAge,
         Rcpp::Named("max") = (int)pSharedData->gwCpdMaxAge
      ),
      Rcpp::Named("num_intensity_groups") = pSharedData->gwNumIntensityGrps,
      Rcpp::Named("cpd_rows_loaded") = (int)pSharedData->glCpdRowsLoaded,
      Rcpp::Named("cpd_rows_skipped") = (int)pSharedData->glCpdRowsSkipped
   );

   pSharedData->release();
   return out;
}

//' @name runSimFromDataFrame
//' @title runSimFromDataFrame method
//' @description runSimFromDataFrame offers a way to configure and run a simulation from an existing R dataframe. It returns a dataframe of simulated smoking histories with the same number of rows and order as the input dataframe.
//' @details On Windows, \code{output_file} (direct disk output) cannot be combined with
//'          multi-threaded execution (\code{num_threads} not equal to \code{1}). The call stops with an error
//'          before loading inputs or writing files. Use the default in-memory DataFrame return value, or set
//'          \code{num_threads <- 1} to write a file.
//' @param dfPopulation The input dataframe with named columns for race, sex, and birth_cohort

Rcpp::RObject SHGInterface::runSimFromDataFrame(Rcpp::DataFrame dfPopulation) {
   return runSimFromDataFrame(dfPopulation, false, R_NilValue);
}

Rcpp::RObject SHGInterface::runSimFromDataFrame(Rcpp::DataFrame dfPopulation,
                                                std::string output_file_path) {
   return runSimFromDataFrame(dfPopulation, false, R_NilValue, output_file_path);
}

Rcpp::RObject SHGInterface::runSimFromDataFrame(Rcpp::DataFrame dfPopulation,
                                                bool attach_run_info,
                                                std::string output_file_path) {
   return runSimFromDataFrame(dfPopulation, attach_run_info, R_NilValue, output_file_path);
}

Rcpp::RObject SHGInterface::runSimFromDataFrame(Rcpp::DataFrame dfPopulation,
                                                bool attach_run_info,
                                                Rcpp::Nullable<Rcpp::List> original_config,
                                                std::string output_file_path) {
   const std::string previous_output_file = output_file;
   output_file = output_file_path;
   try {
      Rcpp::RObject out = runSimFromDataFrame(dfPopulation, attach_run_info, original_config);
      output_file = previous_output_file;
      return out;
   } catch (...) {
      output_file = previous_output_file;
      throw;
   }
}

Rcpp::RObject SHGInterface::runSimFromDataFrame(Rcpp::DataFrame dfPopulation,
                                                bool attach_run_info,
                                                Rcpp::Nullable<Rcpp::List> original_config) {
   Rcpp::List original_cfg;
   if (original_config.isNotNull()) {
      original_cfg = Rcpp::as<Rcpp::List>(original_config.get());
   } else {
      original_cfg = Rcpp::List::create();
   }

   if (!SHGInterface::isValidDataFrame(dfPopulation)) {
      Rcpp::stop("Invalid data frame");
   }

   const bool from_fixed_cohort = next_dataframe_call_is_fixed_cohort_;
   next_dataframe_call_is_fixed_cohort_ = false;
   if (!from_fixed_cohort) {
      last_completed_sim_was_fixed_cohort_ = false;
   }

   int repeat = dfPopulation.nrows();
   
   // Determine if multi-threaded: num_threads == -1 (auto) or > 1
   bool bMultiThreaded = (num_threads != 1);

#if defined(_WIN32)
   // Fail before CreateSharedData / any simulation — disk + MT cannot be validated on Windows.
   if (!output_file.empty() && bMultiThreaded) {
      Rcpp::stop(
         "On Windows, output_file (disk output) cannot be used with multi-threaded execution "
         "(num_threads other than 1). Use runSimFromDataFrame without output_file for an in-memory "
         "DataFrame, or set num_threads = 1 if you must write a file."
      );
   }
#endif

   // Windows R package (MinGW/UCRT): std::async worker threads + thread_local scratch in the
   // shared engine (CalcCigarettesPerDaySwitch) is unreliable in-process. Run segment work on the
   // main thread instead; segments still split RNG substreams (same results as Linux for a given
   // segment layout). Non-Windows keeps std::async for throughput.
#if defined(_WIN32)
   constexpr bool kUseAsyncWorkerThreads = false;
#else
   constexpr bool kUseAsyncWorkerThreads = true;
#endif
   const bool asyncWorkerThreads = bMultiThreaded && kUseAsyncWorkerThreads;
   if (bMultiThreaded && !asyncWorkerThreads) {
      Rcpp::Rcout << "[INFO] Windows: running simulation segments sequentially "
                   << "(parallel std::async workers disabled for DLL stability).\n"
                   << std::flush;
   }

   // Auto-calculate segments if -1 (auto) and using RngStream with multi-threading
   int effectiveSegments = number_of_segments;
   if (number_of_segments == -1) {
      if (rng_strategy == "RngStream" && bMultiThreaded) {
         // Auto-calculate: min(cores * 10, repeat / 1000)
         int numCores = (num_threads == -1) ? std::thread::hardware_concurrency() : num_threads;
         if (numCores < 1) numCores = 1;
         const int MIN_INDIVIDUALS_PER_SEGMENT = 1000;
         const int SEGMENT_MULTIPLIER = 10;
         int maxSegmentsFromCores = numCores * SEGMENT_MULTIPLIER;
         int maxSegmentsFromRepeat = repeat / MIN_INDIVIDUALS_PER_SEGMENT;
         if (maxSegmentsFromRepeat < 1) maxSegmentsFromRepeat = 1;
         effectiveSegments = std::min(maxSegmentsFromCores, maxSegmentsFromRepeat);
         if (effectiveSegments < 1) effectiveSegments = 1;
         Rcpp::Rcout << "  [INFO] Auto-calculated number_of_segments=" << effectiveSegments 
                     << " (cores=" << numCores << ", repeat=" << repeat << ")\n";
         Rcpp::Rcout << "  [INFO] For exact reproduction, set: shg$number_of_segments <- " << effectiveSegments << "\n";
      } else {
         effectiveSegments = 1;  // Default to 1 if single-threaded or MersenneTwister
      }
   }
   
   // Validate RNG strategy restrictions
   if (rng_strategy == "MersenneTwister") {
      if (effectiveSegments > 1) {
         Rcpp::stop("MersenneTwister RNG cannot maintain IID properties with multiple segments. MersenneTwister is restricted to 1 segment. Use RngStream for multiple segments.");
      }
      if (bMultiThreaded) {
         Rcpp::stop("MersenneTwister RNG requires single-threaded execution (num_threads = 1). Use RngStream for multi-threading.");
      }
   }
   
   int n = effectiveSegments; // Number of parallel simulations
   int resolvedThreads = num_threads;
   if (resolvedThreads == -1) {
      resolvedThreads = std::thread::hardware_concurrency();
      if (resolvedThreads < 1) resolvedThreads = 1;
   }
   int effectiveThreadsUsed = 1;
   if (asyncWorkerThreads && n > 1) {
      effectiveThreadsUsed = std::min(resolvedThreads, n);
   }
   // Capture effective runtime settings so getConfig() can round-trip what was actually used.
   has_effective_runtime_config_ = true;
   last_effective_number_of_segments_ = n;
   last_effective_num_threads_ = effectiveThreadsUsed;

   int repeat_per_sim = repeat / n;
   int remainder = repeat % n; // Calculate the remainder

   vector<short> 
      wRaces = Rcpp::as<vector<short>>(dfPopulation["race"]),
      wSexes = Rcpp::as<vector<short>>(dfPopulation["sex"]),
      wYearBirths = Rcpp::as<vector<short>>(dfPopulation["birth_cohort"]);

   // Create shared data once for all segments (major performance optimization)
   string initFile = input_data_folder + "/" + initiation_filename;
   string cessFile = input_data_folder + "/" + cessation_filename;
   string lifeFile = input_data_folder + "/" + mortality_filename_;
   string cpdFile = input_data_folder + "/" + cpd_filename;
   SmokingSimulatorSharedData* pSharedData = Smoking_Simulator::CreateSharedData(
      initFile.c_str(), cessFile.c_str(), lifeFile.c_str(), cpdFile.c_str());

   // Store data shape info for later access via get_data_shape()
   last_num_races = pSharedData->gwNumRaceValues;
   last_num_sexes = pSharedData->gwNumSexValues;
   last_num_cohorts = pSharedData->gwNumBirthCohorts;
   last_min_init_age = pSharedData->gwMinInitiationAge;
   last_max_init_age = pSharedData->gwMaxInitiationAge;
   last_min_cess_age = pSharedData->gwMinCessationAge;
   last_max_cess_age = pSharedData->gwMaxCessationAge;
  last_cpd_min_age = pSharedData->gwCpdMinAge;
  last_cpd_max_age = pSharedData->gwCpdMaxAge;
  last_num_intensity_grps = pSharedData->gwNumIntensityGrps;
  if (last_num_cohorts > 0) {
    last_first_cohort_start = pSharedData->gwYOBCohortStartYrs[0];
    last_first_cohort_end = pSharedData->gwYOBCohortEndYrs[0];
    last_last_cohort_start = pSharedData->gwYOBCohortStartYrs[last_num_cohorts - 1];
    last_last_cohort_end = pSharedData->gwYOBCohortEndYrs[last_num_cohorts - 1];
  }

  if (pSharedData->glCpdRowsSkipped > 0) {
    Rcpp::Rcout << "[INFO] CPD file: " << pSharedData->glCpdRowsLoaded
                << " rows loaded, " << pSharedData->glCpdRowsSkipped
                << " rows skipped (cohort labels not matching initiation cohorts).\n";
  }

   // ============================================================
   // FILE OUTPUT MODE: Write directly to disk like CLI
   // ============================================================
   if (!output_file.empty()) {
      Rcpp::Rcout << "[INFO] Writing results to file: " << output_file << "\n" << std::flush;
      
      // Create temp file paths (use path / operator — avoids broken mixed separators on Windows)
      vector<string> tempFiles;
      std::filesystem::path outDir = std::filesystem::path(output_file).parent_path();
      if (outDir.empty()) outDir = ".";
      
      for (int seg = 0; seg < n; seg++) {
         std::filesystem::path tempPath = outDir / ("shg_segment_" + std::to_string(seg) + ".tmp");
         tempFiles.push_back(tempPath.string());
      }
      
      // Launch segments
      vector<future<void>> futures;
      
      int fileCumOffset = 0;
      for (int seg = 0; seg < n; seg++) {
         int current_repeat = repeat_per_sim + (seg < remainder ? 1 : 0);
         int offset = fileCumOffset;
         fileCumOffset += current_repeat;

         if (asyncWorkerThreads) {
            futures.push_back(async(launch::async, &SHGInterface::runSimSegmentToFile, this,
                                    current_repeat,
                                    ref(wRaces),
                                    ref(wSexes),
                                    ref(wYearBirths),
                                    offset,
                                    tempFiles[seg],
                                    pSharedData,
                                    seg));
         } else {
            runSimSegmentToFile(current_repeat,
                               ref(wRaces),
                               ref(wSexes),
                               ref(wYearBirths),
                               offset,
                               tempFiles[seg],
                               pSharedData,
                               seg);
         }
      }
      
      if (asyncWorkerThreads) {
         for (auto& fut : futures) {
            fut.get();
         }
      }
      
      // Assemble temp files into final output with XML header (matching CLI format)
      // Use first individual's values for the header (mixed populations use "0" as placeholder)
      int headerRace = wRaces.size() > 0 ? wRaces[0] : 0;
      int headerSex = wSexes.size() > 0 ? wSexes[0] : 0;
      int headerYob = wYearBirths.size() > 0 ? wYearBirths[0] : 0;
      bool bAutoSegments = (number_of_segments == -1);
      assembleSegmentFiles(tempFiles, output_file, repeat, headerRace, headerSex, headerYob,
                           n, asyncWorkerThreads, bAutoSegments);
      
      delete pSharedData;
      
      Rcpp::Rcout << "[INFO] Results written to: " << output_file << "\n" << std::flush;

      last_completed_sim_was_fixed_cohort_ = from_fixed_cohort;

      // Return minimal DataFrame with info
      Rcpp::DataFrame stub_df = Rcpp::DataFrame::create(
         Rcpp::Named("info") = Rcpp::CharacterVector::create("Results written to file: " + output_file),
         Rcpp::Named("rows") = Rcpp::IntegerVector::create(repeat)
      );
      return finalizeSimOutput(stub_df, attach_run_info, original_cfg);
   }
   
   // ============================================================
   // MEMORY MODE: Return DataFrame (default)
   // ============================================================
   vector<short> initiationAge(repeat), cessationAge(repeat), ageAtDeath(repeat);
   
   // CPD storage - string formats
   vector<string> cpdString;
   if (cpd_format != "none") {
      cpdString.resize(repeat);
   }

   // Vectors to store futures
   vector<future<void>> futures;
   
   // Launch n simulations in parallel (or sequentially if single-threaded)
   int memCumOffset = 0;
   for (int i = 0; i < n; ++i) {
      int current_repeat_per_sim = repeat_per_sim + (i < remainder ? 1 : 0);
      int offset = memCumOffset;
      memCumOffset += current_repeat_per_sim;

      if (asyncWorkerThreads) {
         futures.push_back(async(launch::async, &SHGInterface::runSimSegment, this,
                                     current_repeat_per_sim,
                                     ref(wRaces),
                                     ref(wSexes),
                                     ref(wYearBirths),
                                     ref(initiationAge),
                                     ref(cessationAge),
                                     ref(ageAtDeath),
                                     ref(cpdString),
                                     offset,
                                     pSharedData));
      }
      else {
         SHGInterface::runSimSegment(current_repeat_per_sim,
                     ref(wRaces),
                     ref(wSexes),
                     ref(wYearBirths),
                     ref(initiationAge),
                     ref(cessationAge),
                     ref(ageAtDeath),
                     ref(cpdString),
                     offset,
                     pSharedData);
      }
    }
    // Wait for all simulations to complete
    if (asyncWorkerThreads) {
      for (auto& fut : futures) {
        fut.get();
      }
    }

   // Clean up shared data
   delete pSharedData;

   // Convert to Rcpp::DataFrame - conditionally include CPD
   Rcpp::IntegerVector initiationAgeVec(initiationAge.begin(), initiationAge.end());
   Rcpp::IntegerVector cessationAgeVec(cessationAge.begin(), cessationAge.end());
   Rcpp::IntegerVector ageAtDeathVec(ageAtDeath.begin(), ageAtDeath.end());

   // Check if input columns are constant (optimization: skip if all same value)
   Rcpp::IntegerVector raceVec = dfPopulation["race"];
   Rcpp::IntegerVector sexVec = dfPopulation["sex"];
   Rcpp::IntegerVector cohortVec = dfPopulation["birth_cohort"];
   
   bool raceConstant = (raceVec.size() > 0 && std::all_of(raceVec.begin(), raceVec.end(), [&](int v) { return v == raceVec[0]; }));
   bool sexConstant = (sexVec.size() > 0 && std::all_of(sexVec.begin(), sexVec.end(), [&](int v) { return v == sexVec[0]; }));
   bool cohortConstant = (cohortVec.size() > 0 && std::all_of(cohortVec.begin(), cohortVec.end(), [&](int v) { return v == cohortVec[0]; }));
   if (cohortConstant) {
      has_last_cohort_year_ = true;
      last_cohort_year_ = cohortVec[0];
   } else {
      has_last_cohort_year_ = false;
      last_cohort_year_ = 0;
   }

   Rcpp::DataFrame df;
   if (cpd_format == "none") {
      // Build DataFrame conditionally - exclude constant columns
      Rcpp::List dfList;
      if (!raceConstant) dfList["race"] = raceVec;
      if (!sexConstant) dfList["sex"] = sexVec;
      if (!cohortConstant) dfList["birth_cohort"] = cohortVec;
      dfList["smoking_initiation_age"] = initiationAgeVec;
      dfList["smoking_cessation_age"] = cessationAgeVec;
      dfList["age_at_death"] = ageAtDeathVec;
      df = Rcpp::DataFrame(dfList);
   } else {
      // String formats (sparse/full) - slower due to R string creation
      SEXP cpdSEXP = PROTECT(Rf_allocVector(STRSXP, repeat));
      for (int i = 0; i < repeat; i++) {
         const std::string& s = cpdString[i];
         SET_STRING_ELT(cpdSEXP, i, Rf_mkCharLen(s.c_str(), s.size()));
      }
      Rcpp::CharacterVector cpdStringVec(cpdSEXP);
      UNPROTECT(1);
      
      // Build DataFrame conditionally - exclude constant columns
      Rcpp::List dfList;
      if (!raceConstant) dfList["race"] = raceVec;
      if (!sexConstant) dfList["sex"] = sexVec;
      if (!cohortConstant) dfList["birth_cohort"] = cohortVec;
      dfList["smoking_initiation_age"] = initiationAgeVec;
      dfList["smoking_cessation_age"] = cessationAgeVec;
      dfList["age_at_death"] = ageAtDeathVec;
      dfList["cigarettes_per_day"] = cpdStringVec;
      df = Rcpp::DataFrame(dfList);
   }

   last_completed_sim_was_fixed_cohort_ = from_fixed_cohort;

    return finalizeSimOutput(df, attach_run_info, original_cfg);
}
//' @name runSimFromFixedValues
//' @title runSimFromFixedValues method
//' @description runSimFromFixedValues offers a way to configure and run a simulation from fixed values for race, sex, and birth year cohort rather than passing a data frame. It returns a dataframe of simulated smoking histories for n individuals.
//' @param repeat The number of individuals to simulate
//' @param race (default = 0 and refers to all races combined)
//' @param sex (0 for male, 1, for female)
//' @param cohort_year (four digit birth cohort year)
Rcpp::RObject SHGInterface::runSimFromFixedValues(int repeat, short wRace, short wSex, short wYearBirth) {
   return runSimFromFixedValues(repeat, wRace, wSex, wYearBirth, false, R_NilValue);
}

Rcpp::RObject SHGInterface::runSimFromFixedValues(int repeat,
                                                  short wRace,
                                                  short wSex,
                                                  short wYearBirth,
                                                  std::string output_file_path) {
   return runSimFromFixedValues(repeat, wRace, wSex, wYearBirth, false, R_NilValue, output_file_path);
}

Rcpp::RObject SHGInterface::runSimFromFixedValues(int repeat,
                                                  short wRace,
                                                  short wSex,
                                                  short wYearBirth,
                                                  bool attach_run_info,
                                                  Rcpp::Nullable<Rcpp::List> original_config,
                                                  std::string output_file_path) {
   const std::string previous_output_file = output_file;
   output_file = output_file_path;
   try {
      Rcpp::RObject out = runSimFromFixedValues(
         repeat, wRace, wSex, wYearBirth, attach_run_info, original_config);
      output_file = previous_output_file;
      return out;
   } catch (...) {
      output_file = previous_output_file;
      throw;
   }
}

Rcpp::RObject SHGInterface::runSimFromFixedValues(int repeat,
                                                  short wRace,
                                                  short wSex,
                                                  short wYearBirth,
                                                  bool attach_run_info,
                                                  Rcpp::Nullable<Rcpp::List> original_config) {
   if (repeat < 1) {
      Rcpp::stop(
         "Requested repeat value %d is invalid. repeat must be >= 1.",
         repeat
      );
   }

   Rcpp::List original_cfg;
   if (original_config.isNotNull()) {
      original_cfg = Rcpp::as<Rcpp::List>(original_config.get());
   } else {
      original_cfg = Rcpp::List::create(
         Rcpp::Named("individuals") = repeat,
         Rcpp::Named("race") = static_cast<int>(wRace),
         Rcpp::Named("sex") = static_cast<int>(wSex),
         Rcpp::Named("cohort_year") = static_cast<int>(wYearBirth));
   }

   string initFile = input_data_folder + "/" + initiation_filename;
   string cessFile = input_data_folder + "/" + cessation_filename;
   string lifeFile = input_data_folder + "/" + mortality_filename_;
   string cpdFile = input_data_folder + "/" + cpd_filename;
   SmokingSimulatorSharedData* pSharedData = Smoking_Simulator::CreateSharedData(
      initFile.c_str(), cessFile.c_str(), lifeFile.c_str(), cpdFile.c_str());

   const int availableRaces = static_cast<int>(pSharedData->gwNumRaceValues);
   const int availableSexes = static_cast<int>(pSharedData->gwNumSexValues);
   const int cohortCount = static_cast<int>(pSharedData->gwNumBirthCohorts);

   if (wRace < 0 || wRace >= availableRaces) {
      pSharedData->release();
      Rcpp::stop(
         "Requested race value %d is not available in the loaded parameter set. "
         "Available race values are 0..%d (count=%d).",
         static_cast<int>(wRace),
         availableRaces - 1,
         availableRaces
      );
   }
   if (wSex < 0 || wSex >= availableSexes) {
      pSharedData->release();
      Rcpp::stop(
         "Requested sex value %d is not available in the loaded parameter set. "
         "Available sex values are 0..%d (count=%d).",
         static_cast<int>(wSex),
         availableSexes - 1,
         availableSexes
      );
   }

   bool cohortAvailable = false;
   for (int i = 0; i < cohortCount; ++i) {
      const short start = pSharedData->gwYOBCohortStartYrs[i];
      const short end = pSharedData->gwYOBCohortEndYrs[i];
      if (wYearBirth >= start && wYearBirth <= end) {
         cohortAvailable = true;
         break;
      }
   }
   if (!cohortAvailable) {
      const int minCohort = cohortCount > 0 ? pSharedData->gwYOBCohortStartYrs[0] : NA_INTEGER;
      const int maxCohort = cohortCount > 0 ? pSharedData->gwYOBCohortEndYrs[cohortCount - 1] : NA_INTEGER;
      pSharedData->release();
      Rcpp::stop(
         "Requested cohort_year %d is not available in the loaded parameter set. "
         "Available cohort range is [%d, %d] across %d cohort windows.",
         static_cast<int>(wYearBirth),
         minCohort,
         maxCohort,
         cohortCount
      );
   }
   pSharedData->release();

   has_last_fixed_run_ = true;
   last_fixed_repeat_ = repeat;
   last_fixed_race_ = wRace;
   last_fixed_sex_ = wSex;

   // Create a DataFrame and populate it with the fixed values
   Rcpp::DataFrame df = Rcpp::DataFrame::create(
      Rcpp::Named("race") = Rcpp::IntegerVector(repeat, wRace),
      Rcpp::Named("sex") = Rcpp::IntegerVector(repeat, wSex),
      Rcpp::Named("birth_cohort") = Rcpp::IntegerVector(repeat, wYearBirth),
      Rcpp::Named("smoking_initiation_age") = Rcpp::IntegerVector(repeat),
      Rcpp::Named("smoking_cessation_age") = Rcpp::IntegerVector(repeat),
      Rcpp::Named("age_at_death") = Rcpp::IntegerVector(repeat),
      Rcpp::Named("cigarettes_per_day") = Rcpp::CharacterVector(repeat)
   );

   next_dataframe_call_is_fixed_cohort_ = true;
   return runSimFromDataFrame(df, attach_run_info, Rcpp::Nullable<Rcpp::List>(original_cfg));
}

bool SHGInterface::isValidDataFrame(Rcpp::DataFrame& dfPopulation) {
   int repeat = dfPopulation.nrows();

   // Check if the required columns (race, sex, birth_cohort) exist in the data frame
   Rcpp::CharacterVector columnNames = dfPopulation.names();
   bool hasRace = false;
   bool hasSex = false;
   bool hasBirthCohort = false;

   for (const auto& columnName : columnNames) {
      if (columnName == "race") {
         hasRace = true;
      }
      else if (columnName == "sex") {
         hasSex = true;
      }
      else if (columnName == "birth_cohort") {
         hasBirthCohort = true;
      }
   }
   // Create a comma-delimited list of column names
   string columnNamesList;
   for (const auto& columnName : columnNames) {
      columnNamesList += string(columnName) + ", ";
   }
   columnNamesList = columnNamesList.substr(0, columnNamesList.length() - 2); // Remove the trailing comma and space

   if (!hasRace || !hasSex || !hasBirthCohort) {
      Rcpp::stop("Found the following columns: " + columnNamesList + ". Missing one or more required columns: race, sex, or birth_cohort");
   }

   // Ensure that the values for race, sex, and birth_cohort are valid
   Rcpp::IntegerVector raceVec = dfPopulation["race"];
   Rcpp::IntegerVector sexVec = dfPopulation["sex"];
   Rcpp::IntegerVector birthCohortVec = dfPopulation["birth_cohort"];

   for (int i = 0; i < repeat; i++) {
      if (sexVec[i] != 0 && sexVec[i] != 1) {
         Rcpp::stop("Invalid value of '" + to_string(sexVec[i]) + "' for sex at index " + to_string(i));
      }
   }

   for (int i = 0; i < repeat; i++) {
      // NOTE: Removed hardcoded 1900 lower bound check (2025-02-XX) to allow earlier birth cohorts
      // (e.g., 1864) that are supported by the underlying C++ code and data files.
      // The C++ code validates against the actual data range via GetMinYearOfBirth().
      // If needed, this check can be restored by uncommenting: birthCohortVec[i] < 1900 ||
      if (birthCohortVec[i] > 2100) {
         Rcpp::stop("Invalid value of '" + to_string(birthCohortVec[i]) + "' for birth_cohort at index " + to_string(i) + ". Birth cohort must be <= 2100.");
      }
   }
   // TODO: review the following; not sure this is the best practice to just return true unless we have an error
   return true;
}

void SHGInterface::runSimSegment(int repeat,
                              vector<short>& wRaces,
                              vector<short>& wSexes,
                              vector<short>& wDateBirths,
                              vector<short>& initiationAge,
                              vector<short>& cessationAge,
                              vector<short>& ageAtDeath,
                              vector<string>& cpdString,
                              int offset,
                              SmokingSimulatorSharedData* pSharedData) {

   FILE *pOutStream = NULL;

   short wYearsAsSmoker;
   short sPersonsInitAge, sPersonsCessAge, sPersonsAgeAtDeath;

   // Use shared data constructor (reuses pre-loaded data, no file I/O per segment)
   short wOutputType = 1; // Not relevant for R
   Smoking_Simulator* qSimulator = new Smoking_Simulator(pSharedData, wOutputType, immediate_cessation_year);
   
   qSimulator->gbSkipValidation = true;     // Skip input validation (inputs pre-validated by R)

   // Set RNG strategy with user-specified seeds or defaults.
   // Match CLI: default gbSkipOversampling (oversampling ON). For RngStream, parallel
   // main.cpp uses BufferedRngStreamRNG(base, 10000, true) per segment — use the same here
   // so multi-segment R matches the CLI segment workers (buffer preserves MRG32k3a sequence).
   if (rng_strategy == "MersenneTwister") {
      qSimulator->gbSkipOversampling = false;  // MT: keep oversampling for backwards compatibility
      if (mt_seeds.size() == 4) {
         qSimulator->setRNGStrategy(new MersenneTwisterRNG(
            mt_seed_to_engine_arg(mt_seeds[0]),
            mt_seed_to_engine_arg(mt_seeds[1]),
            mt_seed_to_engine_arg(mt_seeds[2]),
            mt_seed_to_engine_arg(mt_seeds[3])));
      } else {
         qSimulator->setRNGStrategy(new MersenneTwisterRNG(1898587603, 1468371936, 1551308340, 1590227640));
      }
   }
   else if (rng_strategy == "RngStream") {
      qSimulator->gbSkipOversampling = false;  // align with CLI RunWebVersion (OversamplePRNGs each person)
      RngStreamRNG* baseRng;
      if (rngstream_seed.size() == 6) {
         unsigned long seed_array[6];
         for (int i = 0; i < 6; i++) {
            seed_array[i] = rngstream_seed[i];
         }
         baseRng = new RngStreamRNG(seed_array);
      } else {
         baseRng = new RngStreamRNG();
      }
      BufferedRngStreamRNG* bufferedRng = new BufferedRngStreamRNG(baseRng, 10000, true);
      qSimulator->setRNGStrategy(bufferedRng);
   }
   else
      Rcpp::stop("Invalid RNG strategy or strategy not yet implemented");

   // Segment index for RNG: with the same per-segment counts as main.cpp (repsPerSegment +
   // (seg < remainder), cumulative offset), offset/repeat equals the segment index (integer div).
   int segment_number = offset / repeat;
   qSimulator->incrementSubstreams(segment_number);

   // Pre-check cpd_format to avoid per-iteration string comparison
   bool needCpd = (cpd_format != "none");
   bool useSparse = (cpd_format == "sparse");
   
   for (int j = 0; j < repeat; j++)
   {
      int k = offset + j;
      qSimulator->RunSimulationSingle(wRaces[k], wSexes[k], wDateBirths[k], pOutStream);

      sPersonsInitAge = qSimulator->GetPersonsInitAge();
      sPersonsCessAge = qSimulator->GetPersonsCessAge();
      sPersonsAgeAtDeath = qSimulator->GetPersonsAgeAtDeath();

      initiationAge[k] = sPersonsInitAge;
      cessationAge[k] = sPersonsCessAge;
      ageAtDeath[k] = sPersonsAgeAtDeath;

      // Only process CPD if needed (cpd_format != "none")
      if (needCpd && sPersonsInitAge != -999) {
         double* dPersonsCPDbyAge = qSimulator->GetPersonsCPDbyAge();
         if (sPersonsCessAge == -999)
            wYearsAsSmoker = wSIM_CUTOFF_YEAR - (wDateBirths[k] + sPersonsInitAge) + 1;
         else
            wYearsAsSmoker = sPersonsCessAge - sPersonsInitAge + 1;
         
         // Stack buffer: avoid thread_local in DLL workers on Windows (see runSimSegmentToFile note).
         char cpdBuf[2048];
         char* ptr = cpdBuf;
         
         for (int i = 0; i < wYearsAsSmoker; i++) {
            int age = i + sPersonsInitAge;
            if (age < 100) {
               int cpdVal = (int)dPersonsCPDbyAge[i];
               if (ptr != cpdBuf) {
                  *ptr++ = ',';
                  *ptr++ = ' ';
               }
               if (useSparse) {
                  ptr = fast_itoa(cpdVal, ptr);
               } else {
                  // Full format: "age (cpd)"
                  ptr = fast_itoa(age, ptr);
                  *ptr++ = ' ';
                  *ptr++ = '(';
                  ptr = fast_itoa(cpdVal, ptr);
                  *ptr++ = ')';
               }
            }
         }
         *ptr = '\0';
         cpdString[k] = cpdBuf;
      }
   }
   // fclose(pOutStream); # this caused a segfault in Ubuntu and is probably not needed because there is no output file for the Rcpp version
}

// File output mode: writes directly to disk like CLI, reusing WriteAsData (DRY)
void SHGInterface::runSimSegmentToFile(int repeat,
                                       vector<short>& wRaces,
                                       vector<short>& wSexes,
                                       vector<short>& wYearBirths,
                                       int offset,
                                       const string& tempFilePath,
                                       SmokingSimulatorSharedData* pSharedData,
                                       int segmentNumber) {
   
   // Binary mode: avoid Windows CRT translating \n -> \r\n (breaks line-based tests / XML tags).
   FILE* pOutFile = fopen(tempFilePath.c_str(), "wb");
   if (!pOutFile) {
      // Throw C++ exception: Rcpp::stop() is not safe to call from std::async threads.
      throw std::runtime_error("Could not open temp file for writing: " + tempFilePath);
   }
   
   // Heap-allocated I/O buffer (4 MB) for better disk throughput.
   // Previously a static thread_local, which crashes on Windows when called from std::async
   // threads inside a dynamically-loaded DLL (MinGW/UCRT TLS initialisation limitation).
   std::vector<char> fileBuffer(4 * 1024 * 1024);
   setvbuf(pOutFile, fileBuffer.data(), _IOFBF, fileBuffer.size());
   
   // Create simulator using shared data
   short wOutputType = 1;
   Smoking_Simulator* qSimulator = new Smoking_Simulator(pSharedData, wOutputType, immediate_cessation_year);
   qSimulator->gbSkipValidation = true;
   
   // Set RNG strategy (match RunWebVersion / CLI; see runSimSegment)
   if (rng_strategy == "MersenneTwister") {
      qSimulator->gbSkipOversampling = false;  // MT: keep oversampling for backwards compatibility
      if (mt_seeds.size() == 4) {
         qSimulator->setRNGStrategy(new MersenneTwisterRNG(
            mt_seed_to_engine_arg(mt_seeds[0]),
            mt_seed_to_engine_arg(mt_seeds[1]),
            mt_seed_to_engine_arg(mt_seeds[2]),
            mt_seed_to_engine_arg(mt_seeds[3])));
      } else {
         qSimulator->setRNGStrategy(new MersenneTwisterRNG(1898587603, 1468371936, 1551308340, 1590227640));
      }
   } else if (rng_strategy == "RngStream") {
      qSimulator->gbSkipOversampling = false;  // align with CLI RunWebVersion
      RngStreamRNG* baseRng;
      if (rngstream_seed.size() == 6) {
         unsigned long seed_array[6];
         for (int i = 0; i < 6; i++) {
            seed_array[i] = rngstream_seed[i];
         }
         baseRng = new RngStreamRNG(seed_array);
      } else {
         baseRng = new RngStreamRNG();
      }
      BufferedRngStreamRNG* bufferedRng = new BufferedRngStreamRNG(baseRng, 10000, true);
      qSimulator->setRNGStrategy(bufferedRng);
   }
   
   // Advance RNG substreams for this segment
   qSimulator->incrementSubstreams(segmentNumber);
   
   // Run simulations and write each individual using CLI's WriteAsData (DRY)
   for (int j = 0; j < repeat; j++) {
      int k = offset + j;
      qSimulator->RunSimulationSingle(wRaces[k], wSexes[k], wYearBirths[k], pOutFile);
   }
   
   fclose(pOutFile);
   delete qSimulator;
}

// Assemble temp segment files into final output with XML header (matching CLI format)
void SHGInterface::assembleSegmentFiles(const vector<string>& tempFiles, const string& outputFile,
                                        int repeat, int race, int sex, int yob,
                                        int effectiveSegments, bool bMultiThreaded, bool bAutoSegments) {
   FILE* pOutFile = fopen(outputFile.c_str(), "wb");
   if (!pOutFile) {
      Rcpp::stop("Could not open output file for writing: " + outputFile);
   }
   
   // Write XML metadata header (matching CLI format for DRY)
   string initFile = input_data_folder + "/" + initiation_filename;
   string cessFile = input_data_folder + "/" + cessation_filename;
   string lifeFile = input_data_folder + "/" + mortality_filename_;
   string cpdFile = input_data_folder + "/" + cpd_filename;
   
   // Build seed string
   string seedStr = "";
   if (rng_strategy == "RngStream") {
      for (size_t i = 0; i < rngstream_seed.size() && i < 6; i++) {
         if (i > 0) seedStr += ",";
         seedStr += to_string(rngstream_seed[i]);
      }
   }
   
   // Build MT seeds as strings
   string mtInit = mt_seeds.size() > 0 ? to_string(mt_seeds[0]) : "";
   string mtCess = mt_seeds.size() > 1 ? to_string(mt_seeds[1]) : "";
   string mtOcd = mt_seeds.size() > 2 ? to_string(mt_seeds[2]) : "";
   string mtMisc = mt_seeds.size() > 3 ? to_string(mt_seeds[3]) : "";
   string cessYearStr = to_string(immediate_cessation_year);
   
   // Write RunInfo (same format as CLI)
   WriteRunInfoTag(pOutFile, SHG_CORE_VERSION, 
                   mtInit.c_str(), mtCess.c_str(), mtOcd.c_str(), mtMisc.c_str(),
                   cessYearStr.c_str(), initFile.c_str(), cessFile.c_str(), lifeFile.c_str(),
                   "", cpdFile.c_str(), outputFile.c_str(), "",
                   rng_strategy.c_str(), seedStr.c_str(), "R wrapper",
                   effectiveSegments, num_threads, bMultiThreaded, bAutoSegments);
   
   // Write simulation open tags
   WriteSimulationOpenTag(pOutFile, false);
   
   // Write input tag
   string raceStr = to_string(race);
   string sexStr = to_string(sex);
   string yobStr = to_string(yob);
   string repeatStr = to_string(repeat);
   WriteInputTag(pOutFile, raceStr.c_str(), sexStr.c_str(), yobStr.c_str(), repeatStr.c_str(), false);
   
   WriteToFile(pOutFile, "<RUN>\n");
   
   // Copy segment data
   char buffer[65536];
   for (const auto& tempPath : tempFiles) {
      FILE* pIn = fopen(tempPath.c_str(), "rb");
      if (pIn) {
         size_t n;
         while ((n = fread(buffer, 1, sizeof(buffer), pIn)) > 0) {
            fwrite(buffer, 1, n, pOutFile);
         }
         fclose(pIn);
         std::filesystem::remove(tempPath);
      }
   }
   
   // Write closing tags
   WriteSimulationCloseTag(pOutFile, false);
   
   fclose(pOutFile);
}

bool SHGInterface::fileExists(const char* filename) {
   ifstream file(filename);
   return file.good();
}
//' @name LegacyRunWebVersion
//' @title LegacyRunWebVersion method
//' @description This method offers a way to configure and run a simulation from an input configuration file. Rather than return a R DataFrame, it produces results in an output file. It works in the same as calling the CLI version of the Smoking History Generator with a single input file parameter.
//' @param input_file_name Path to a Legacy web-style configuration file. Paths inside the file are resolved relative to the R process working directory (the \code{input_data_folder} property is ignored). Sample text configs live under \code{tests/testdata/legacy-web-examples/} in the package source; for installed use, build a config with absolute paths from \code{system.file("extdata", "2018", package = "SmokingHistoryGenerator")}.
//' @examples
//' shg <- new(SHGInterface)
//' d <- system.file("extdata", "2018", package = "SmokingHistoryGenerator")
//' tf <- tempfile(fileext = ".txt")
//' writeLines(c(
//'   "RNGSTRATEGY=RngStream",
//'   "RNGSTREAM_SEED=12345,12345,12345,12345,12345,12345",
//'   "RACE=0", "SEX=0", "YOB=1950", "CESSATION_YR=0", "REPEAT=100",
//'   paste0("INIT_PROB=", file.path(d, "smoking", "initiation.csv")),
//'   paste0("CESS_PROB=", file.path(d, "smoking", "cessation.csv")),
//'   paste0("MORTALITY_PROB=", file.path(d, "mortality", "acm.csv")),
//'   paste0("CPD_DATA=", file.path(d, "smoking", "cpd.csv")),
//'   paste0("OUTPUTFILE=", tempfile("out_", fileext = ".txt")),
//'   paste0("ERRORFILE=", tempfile("err_", fileext = ".txt"))
//' ), tf)
//' shg$LegacyRunWebVersion(tf)
void SHGInterface::LegacyRunWebVersion(const char *sInputFileName)
{
   // Paths inside config file are relative to the current working directory
   std::filesystem::path currentPath = std::filesystem::current_path();
   Rcpp::Rcout << "Current working directory: " << currentPath << std::endl;
   Rcpp::Rcout << "Note: the input_data_folder is ignored with LegacyRunWebVersion because it relies on the paths in the config file." << std::endl;
   RunWebVersion(sInputFileName);
   return;
}

//' Get current SHG configuration
//' @name getConfig
//' @title Get SHG Configuration
//' @description Returns the current configuration of the SHG instance as an R list. Can include debug information when debug=TRUE.
//' @param debug Logical. If TRUE, includes additional debug information such as RNG state fingerprint, package version, system info, and memory usage. If not provided, defaults to FALSE.
//' @return A list containing the current intent configuration including: config_version, rng_strategy, number_of_segments, num_threads, seeds, input file paths (including mortality_filename), smok_params_source, mort_params_source, and mort_params_type (from load_params, else NA), immediate_cessation_year, inferred cohort_year (single-cohort runs; otherwise NA), repeat/race/sex after runSimFromFixedValues (otherwise NA), and timestamp. This method returns currently applied values (including unresolved auto values such as -1 for segments/threads). Use \code{getReproConfig()} to export effective runtime values from the last completed simulation. seeds always returns concrete values (explicit user seeds or defaults). If debug=TRUE, also includes rng_state_fingerprint, package_version, package_source, r_version, platform, and memory_usage.
Rcpp::List SHGInterface::buildConfig(bool debug, bool use_effective_runtime, bool require_effective_runtime) {
   if (require_effective_runtime && !has_effective_runtime_config_) {
      Rcpp::stop(
         "No completed simulation is available to export reproducibility config. "
         "Run runSimFromFixedValues() or runSimFromDataFrame() first."
      );
   }

   Rcpp::List config;
   
   // Config version for future compatibility
   config["config_version"] = "1.0";
   
  // Basic configuration
  config["rng_strategy"] = rng_strategy;
  const bool use_effective = use_effective_runtime && has_effective_runtime_config_;
  config["number_of_segments"] = use_effective ? last_effective_number_of_segments_ : number_of_segments;
  // Reproducibility export (getReproConfig): omit num_threads — outcomes must not depend on
  // thread count given fixed seeds and effective segment count; only segments are stored.
  if (!(use_effective_runtime && require_effective_runtime)) {
     config["num_threads"] = use_effective ? last_effective_num_threads_ : num_threads;
  }
  
  // Get seeds using get_current_seeds(). Prefer integer output for whole values
  // so YAML/JSON serialization does not render confusing trailing ".0".
  Rcpp::NumericVector seeds = get_current_seeds();
  bool can_emit_integer_seeds = true;
  for (int i = 0; i < seeds.size(); ++i) {
     const double value = seeds[i];
     if (!R_FINITE(value)) {
        can_emit_integer_seeds = false;
        break;
     }
     const double rounded = std::round(value);
     if (std::fabs(value - rounded) > 0.0 ||
         rounded < static_cast<double>(std::numeric_limits<int>::min()) ||
         rounded > static_cast<double>(std::numeric_limits<int>::max())) {
        can_emit_integer_seeds = false;
        break;
     }
  }
  if (can_emit_integer_seeds) {
     Rcpp::IntegerVector seed_ints(seeds.size());
     for (int i = 0; i < seeds.size(); ++i) {
        seed_ints[i] = static_cast<int>(std::llround(seeds[i]));
     }
     config["seeds"] = seed_ints;
  } else {
     config["seeds"] = seeds;
  }
   
   // Input file configuration
   config["input_data_folder"] = input_data_folder;
  config["output_file"] = output_file;
   config["initiation_filename"] = initiation_filename;
   config["cessation_filename"] = cessation_filename;
   config["mortality_filename"] = mortality_filename_;
   config["cpd_filename"] = cpd_filename;
   config["immediate_cessation_year"] = immediate_cessation_year;
  if (has_last_cohort_year_) {
     config["cohort_year"] = last_cohort_year_;
  } else {
     config["cohort_year"] = Rcpp::IntegerVector::create(NA_INTEGER);
  }
  if (has_last_fixed_run_) {
     config["repeat"] = last_fixed_repeat_;
     config["race"] = static_cast<int>(last_fixed_race_);
     config["sex"] = static_cast<int>(last_fixed_sex_);
  } else {
     config["repeat"] = Rcpp::IntegerVector::create(NA_INTEGER);
     config["race"] = Rcpp::IntegerVector::create(NA_INTEGER);
     config["sex"] = Rcpp::IntegerVector::create(NA_INTEGER);
  }

   if (smok_params_source_.empty()) {
      config["smok_params_source"] = Rcpp::CharacterVector::create(NA_STRING);
   } else {
      config["smok_params_source"] = smok_params_source_;
   }
   if (mort_params_source_.empty()) {
      config["mort_params_source"] = Rcpp::CharacterVector::create(NA_STRING);
   } else {
      config["mort_params_source"] = mort_params_source_;
   }
   if (mort_params_type_.empty()) {
      config["mort_params_type"] = Rcpp::CharacterVector::create(NA_STRING);
   } else {
      config["mort_params_type"] = mort_params_type_;
   }
   
   // Timestamp
   Rcpp::Function Sys_time("Sys.time");
   Rcpp::Function format("format");
   Rcpp::RObject time_obj = Sys_time();
   Rcpp::String timestamp = Rcpp::as<std::string>(format(time_obj, Rcpp::_["format"] = "%Y-%m-%d %H:%M:%S"));
   config["timestamp"] = timestamp;

  if (use_effective_runtime) {
     try {
        Rcpp::Environment pkg_env = Rcpp::Environment::namespace_env("SmokingHistoryGenerator");
        Rcpp::Function package_repro = pkg_env[".shg_package_repro_identity"];
        config["package_repro"] = package_repro(
            Rcpp::Named("core_version") = std::string(SHG_CORE_VERSION),
            Rcpp::Named("minimal") = Rcpp::wrap(true));
     } catch(...) {
        config["package_repro"] = Rcpp::List::create();
     }
  }
   
   // Debug information
   if (debug) {
      // RNG state fingerprint
      Rcpp::NumericVector rng_fingerprint = get_rng_state_fingerprint();
      config["rng_state_fingerprint"] = rng_fingerprint;
      
      // Package version
      try {
         Rcpp::Environment utils("package:utils");
         Rcpp::Function packageVersion = utils["packageVersion"];
         Rcpp::RObject pkg_ver_obj = packageVersion("SmokingHistoryGenerator");
         Rcpp::Function as_character("as.character");
         Rcpp::RObject pkg_ver_str_obj = as_character(pkg_ver_obj);
         Rcpp::CharacterVector pkg_ver_cv = Rcpp::as<Rcpp::CharacterVector>(pkg_ver_str_obj);
         if (pkg_ver_cv.size() > 0) {
            config["package_version"] = Rcpp::as<std::string>(pkg_ver_cv[0]);
         } else {
            config["package_version"] = "unknown";
         }
      } catch(...) {
         config["package_version"] = "unknown";
      }
      
      // Package source (installation path)
      try {
         Rcpp::Environment base("package:base");
         Rcpp::Function system_file = base["system.file"];
         Rcpp::RObject pkg_path_obj = system_file("", Rcpp::_["package"] = "SmokingHistoryGenerator");
         Rcpp::StringVector pkg_path = Rcpp::as<Rcpp::StringVector>(pkg_path_obj);
         if (pkg_path.size() > 0) {
            config["package_source"] = Rcpp::as<std::string>(pkg_path[0]);
         } else {
            config["package_source"] = "unknown";
         }
      } catch(...) {
         config["package_source"] = "unknown";
      }
      
      // R version and platform
      try {
         Rcpp::Environment base_env = Rcpp::Environment::base_env();
         Rcpp::List r_version_list = Rcpp::as<Rcpp::List>(base_env["R.version"]);
         Rcpp::RObject version_string_obj = r_version_list["version.string"];
         Rcpp::RObject platform_obj = r_version_list["platform"];
         config["r_version"] = Rcpp::as<std::string>(version_string_obj);
         config["platform"] = Rcpp::as<std::string>(platform_obj);
      } catch(...) {
         config["r_version"] = "unknown";
         config["platform"] = "unknown";
      }
      
      // Memory usage
      try {
         Rcpp::Function gc("gc");
         Rcpp::RObject mem_info_obj = gc();
         Rcpp::List mem_info = Rcpp::as<Rcpp::List>(mem_info_obj);
         config["memory_usage"] = mem_info;
      } catch(...) {
         config["memory_usage"] = Rcpp::List::create();
      }
   }
   
   return config;
}

Rcpp::List SHGInterface::getConfig(bool debug) {
   return buildConfig(debug, false, false);
}

// Wrapper method without debug parameter (defaults to false)
Rcpp::List SHGInterface::getConfig() {
   return getConfig(false);
}

//' Get reproducibility-focused SHG configuration from last run
//' @name getReproConfig
//' @title Get Reproducibility Configuration
//' @description Returns a configuration list that captures effective runtime settings from the last completed simulation.
//' @param debug Logical. If TRUE, includes additional debug information such as RNG state fingerprint, package version, system info, and memory usage. If not provided, defaults to FALSE.
//' @return A list like \code{getConfig()} for the last completed simulation, but with
//' \code{number_of_segments} as the effective segment count used and \strong{without}
//' \code{num_threads} (thread count must not affect simulation outcomes for fixed seeds
//' and segment layout; consumers default to auto threads when reloading). Errors if no
//' simulation has completed on the instance.
//' @examples
//' shg <- new(SHGInterface)
//' shg$input_data_folder <- system.file("extdata", "2018", package = "SmokingHistoryGenerator")
//' shg$runSimFromFixedValues(500, 0, 0, 1950)
//' repro <- shg$getReproConfig()
Rcpp::List SHGInterface::getReproConfig(bool debug) {
   return buildConfig(debug, true, true);
}

// Wrapper method without debug parameter (defaults to false)
Rcpp::List SHGInterface::getReproConfig() {
   return getReproConfig(false);
}

//' Configure SHG instance from config object
//' @name useConfig
//' @title Use SHG Configuration
//' @description Configures an existing SHG instance from a configuration object (typically obtained from getConfig()).
//' @param config A list containing configuration parameters. Must include config_version. All parameters are validated.
//' @details This method validates the config_version and all parameters before setting them. Unknown fields are warned about but allowed for future compatibility. Missing optional fields use defaults. Fields are applied in an order suitable for round-trips from getConfig(): number_of_segments and num_threads are set before rng_strategy (so switching to Mersenne Twister does not message when the saved list already has single-threaded settings), then seeds, then paths and other options. If the list has deprecated \code{run_multi_threaded} but no \code{num_threads}, it is mapped: FALSE -> \code{num_threads = 1}, TRUE -> \code{num_threads = -1}. If both are present, \code{num_threads} wins. If the list updates local input paths (\code{input_data_folder} or any per-table filename) but omits \code{smok_params_source}, \code{mort_params_source}, and \code{mort_params_type}, any previously recorded bundle provenance is cleared for the omitted key(s) so metadata cannot refer to an older zip after retargeting inputs.
void SHGInterface::useConfig(Rcpp::List config) {
   last_completed_sim_was_fixed_cohort_ = false;
   has_effective_runtime_config_ = false;
   last_effective_number_of_segments_ = -1;
   last_effective_num_threads_ = -1;

   // Validate config_version when provided (missing is treated as current format).
   if (config.containsElementNamed("config_version")) {
      std::string config_ver = Rcpp::as<std::string>(config["config_version"]);
      if (config_ver != "1.0") {
         Rcpp::Function warning("warning");
         warning("Config version " + config_ver + " may not be fully supported. Current version is 1.0.", Rcpp::Named("call.") = false);
      }
   }
   
   // Apply segment/thread counts before rng_strategy so switching to MersenneTwister
   // does not spuriously message (saved configs from getConfig() list threads first).
   if (config.containsElementNamed("number_of_segments")) {
      set_number_of_segments(Rcpp::as<int>(config["number_of_segments"]));
   }
   const bool had_num_threads = config.containsElementNamed("num_threads");
   if (had_num_threads) {
      set_num_threads(Rcpp::as<int>(config["num_threads"]));
   } else if (config.containsElementNamed("run_multi_threaded")) {
      const bool old_mt = Rcpp::as<bool>(config["run_multi_threaded"]);
      set_num_threads(old_mt ? -1 : 1);
      Rcpp::Function warning("warning");
      warning(
         std::string("Deprecated 'run_multi_threaded' applied as num_threads = ") +
            (old_mt ? "-1 (auto)" : "1 (single-threaded)") +
            ". Prefer saving configs with num_threads.",
         Rcpp::Named("call.") = false);
   }
   if (had_num_threads && config.containsElementNamed("run_multi_threaded")) {
      Rcpp::Function warning("warning");
      warning(
         "'run_multi_threaded' is deprecated and ignored when 'num_threads' is present.",
         Rcpp::Named("call.") = false);
   }
   if (config.containsElementNamed("rng_strategy")) {
      set_rng_strategy(Rcpp::as<std::string>(config["rng_strategy"]));
   }

   // Set seeds if provided (after rng_strategy; seed setters depend on strategy)
   if (config.containsElementNamed("seeds")) {
      Rcpp::NumericVector seeds = config["seeds"];
      if (seeds.size() > 0) {
         if (rng_strategy == "MersenneTwister" && seeds.size() == 4) {
            set_mt_seeds(seeds);
         } else if (rng_strategy == "RngStream" && seeds.size() == 6) {
            set_rngstream_seed(seeds);
         } else if (seeds.size() > 0) {
            Rcpp::Function warning("warning");
            warning("Seeds provided but size doesn't match RNG strategy requirements. MersenneTwister requires 4 seeds, RngStream requires 6 seeds.", Rcpp::Named("call.") = false);
         }
      }
   }
  
  if (config.containsElementNamed("input_data_folder")) {
      set_input_data_folder(Rcpp::as<std::string>(config["input_data_folder"]));
   }
  if (config.containsElementNamed("output_file")) {
      set_output_file(Rcpp::as<std::string>(config["output_file"]));
   }
   
   if (config.containsElementNamed("initiation_filename")) {
      set_initiation_filename(Rcpp::as<std::string>(config["initiation_filename"]));
   }
   
   if (config.containsElementNamed("cessation_filename")) {
      set_cessation_filename(Rcpp::as<std::string>(config["cessation_filename"]));
   }
   
   if (config.containsElementNamed("mortality_filename")) {
      set_mortality_filename(Rcpp::as<std::string>(config["mortality_filename"]));
   }
   
   if (config.containsElementNamed("cpd_filename")) {
      set_cpd_filename(Rcpp::as<std::string>(config["cpd_filename"]));
   }
   
  if (config.containsElementNamed("immediate_cessation_year")) {
      set_immediate_cessation_year(Rcpp::as<int>(config["immediate_cessation_year"]));
   }
   if (config.containsElementNamed("cohort_year")) {
      Rcpp::IntegerVector cohort = config["cohort_year"];
      if (cohort.size() >= 1 && cohort[0] != NA_INTEGER) {
         has_last_cohort_year_ = true;
         last_cohort_year_ = cohort[0];
      } else {
         has_last_cohort_year_ = false;
         last_cohort_year_ = 0;
      }
   } else {
      has_last_cohort_year_ = false;
      last_cohort_year_ = 0;
   }

   if (config.containsElementNamed("repeat") && config.containsElementNamed("race") &&
       config.containsElementNamed("sex")) {
      Rcpp::IntegerVector rv_repeat = config["repeat"];
      Rcpp::IntegerVector rv_race = config["race"];
      Rcpp::IntegerVector rv_sex = config["sex"];
      if (rv_repeat.size() >= 1 && rv_race.size() >= 1 && rv_sex.size() >= 1 &&
          rv_repeat[0] != NA_INTEGER && rv_race[0] != NA_INTEGER && rv_sex[0] != NA_INTEGER) {
         has_last_fixed_run_ = true;
         last_fixed_repeat_ = rv_repeat[0];
         last_fixed_race_ = static_cast<short>(rv_race[0]);
         last_fixed_sex_ = static_cast<short>(rv_sex[0]);
      } else {
         has_last_fixed_run_ = false;
         last_fixed_repeat_ = 0;
         last_fixed_race_ = 0;
         last_fixed_sex_ = 0;
      }
   } else {
      has_last_fixed_run_ = false;
      last_fixed_repeat_ = 0;
      last_fixed_race_ = 0;
      last_fixed_sex_ = 0;
   }

   // Provenance is only updated when keys are present; if the caller retargets local
   // input paths without also supplying bundle metadata, drop stale zip/mortality hints.
   const bool has_smok_params_source_key = config.containsElementNamed("smok_params_source");
   const bool has_mort_params_source_key = config.containsElementNamed("mort_params_source");
   const bool has_mort_params_type_key = config.containsElementNamed("mort_params_type");
   const bool touched_local_input_paths =
      config.containsElementNamed("input_data_folder") ||
      config.containsElementNamed("initiation_filename") ||
      config.containsElementNamed("cessation_filename") ||
      config.containsElementNamed("mortality_filename") ||
      config.containsElementNamed("cpd_filename");

   if (touched_local_input_paths) {
      if (!has_smok_params_source_key) {
         smok_params_source_.clear();
      }
      if (!has_mort_params_source_key) {
         mort_params_source_.clear();
      }
      if (!has_mort_params_type_key) {
         mort_params_type_.clear();
      }
   }

   if (has_smok_params_source_key) {
      Rcpp::CharacterVector cv = config["smok_params_source"];
      Rcpp::LogicalVector na = Rcpp::is_na(cv);
      if (cv.size() >= 1 && !na[0]) {
         smok_params_source_ = Rcpp::as<std::string>(cv);
      } else {
         smok_params_source_.clear();
      }
   }
   if (has_mort_params_source_key) {
      Rcpp::CharacterVector cv = config["mort_params_source"];
      Rcpp::LogicalVector na = Rcpp::is_na(cv);
      if (cv.size() >= 1 && !na[0]) {
         mort_params_source_ = Rcpp::as<std::string>(cv);
      } else {
         mort_params_source_.clear();
      }
   }
   if (has_mort_params_type_key) {
      Rcpp::CharacterVector cv = config["mort_params_type"];
      Rcpp::LogicalVector na = Rcpp::is_na(cv);
      if (cv.size() >= 1 && !na[0]) {
         mort_params_type_ = Rcpp::as<std::string>(cv);
      } else {
         mort_params_type_.clear();
      }
   }
  
  // Warn about unknown fields (but allow for future compatibility)
   std::vector<std::string> known_fields = {
    "config_version", "rng_strategy", "number_of_segments", "num_threads", "run_multi_threaded",
    "seeds", "input_data_folder", "output_file", "initiation_filename", "cessation_filename",
    "mortality_filename", "cpd_filename", "immediate_cessation_year",
    "cohort_year", "repeat", "race", "sex", "timestamp",
    "individuals", "mortality",
    "smok_params_source", "mort_params_source", "mort_params_type", "package_repro",
    "rng_state_fingerprint", "package_version", "package_source", "r_version",
      "platform", "memory_usage"
   };
   
   Rcpp::CharacterVector config_names = config.names();
   for (int i = 0; i < config_names.size(); i++) {
      std::string field_name = Rcpp::as<std::string>(config_names[i]);
      bool is_known = false;
      for (const auto& known : known_fields) {
         if (field_name == known) {
            is_known = true;
            break;
         }
      }
      if (!is_known) {
         Rcpp::Function warning("warning");
         warning("Unknown config field: " + field_name + ". This field will be ignored.", Rcpp::Named("call.") = false);
      }
   }
}

RCPP_MODULE(SmokingSimulator) {
   using namespace Rcpp;

// Rcpp_SHGInterface
//' Rcpp SHG Interface Class
//' @name Rcpp_SHGInterface
//' @title Rcpp SHG Interface Class
//' @export
//' @description This module provides an Rcpp interface to the Smoking History Generator (SHG) application, including intent-oriented config methods (\code{getConfig}/\code{useConfig}) and reproducibility export (\code{getReproConfig}).
   class_<SHGInterface>("SHGInterface")
       .constructor("Create a new SHGInterface instance")
       .constructor<Rcpp::List>("Create a new SHGInterface instance with optional config parameter")
       .method("runSimFromFixedValues",
               (Rcpp::RObject(SHGInterface::*)(int, short, short, short)) &SHGInterface::runSimFromFixedValues,
               "Generates a data frame of simulated smoking histories for n individuals (or a bundle when attach_run_info is used)")
      .method("runSimFromFixedValues",
              (Rcpp::RObject(SHGInterface::*)(int, short, short, short, std::string))
                 &SHGInterface::runSimFromFixedValues,
              "Same as 4-arg runSimFromFixedValues, but writes to output_file path when provided")
       .method("runSimFromFixedValues",
               (Rcpp::RObject(SHGInterface::*)(int, short, short, short, bool, Rcpp::Nullable<Rcpp::List>))
                  &SHGInterface::runSimFromFixedValues,
               "Same as 4-arg runSimFromFixedValues; if attach_run_info is TRUE, returns list(results, original_config, repro_config, run_info)")
      .method("runSimFromFixedValues",
              (Rcpp::RObject(SHGInterface::*)(int, short, short, short, bool, Rcpp::Nullable<Rcpp::List>, std::string))
                 &SHGInterface::runSimFromFixedValues,
              "Same as 6-arg runSimFromFixedValues with optional output_file path")
       .method("runSimFromDataFrame",
               (Rcpp::RObject(SHGInterface::*)(Rcpp::DataFrame)) &SHGInterface::runSimFromDataFrame,
               "Generates a data frame of simulated smoking histories for n individuals")
      .method("runSimFromDataFrame",
              (Rcpp::RObject(SHGInterface::*)(Rcpp::DataFrame, std::string))
                 &SHGInterface::runSimFromDataFrame,
              "Same as 1-arg runSimFromDataFrame, but writes to output_file path when provided")
      .method("runSimFromDataFrame",
              (Rcpp::RObject(SHGInterface::*)(Rcpp::DataFrame, bool, std::string))
                 &SHGInterface::runSimFromDataFrame,
              "Same as 1-arg runSimFromDataFrame with attach_run_info and output_file path")
       .method("runSimFromDataFrame",
               (Rcpp::RObject(SHGInterface::*)(Rcpp::DataFrame, bool, Rcpp::Nullable<Rcpp::List>))
                  &SHGInterface::runSimFromDataFrame,
               "Same as 1-arg runSimFromDataFrame; optional bundle when attach_run_info is TRUE")
      .method("runSimFromDataFrame",
              (Rcpp::RObject(SHGInterface::*)(Rcpp::DataFrame, bool, Rcpp::Nullable<Rcpp::List>, std::string))
                 &SHGInterface::runSimFromDataFrame,
              "Same as 3-arg runSimFromDataFrame with optional output_file path")
       .method("LegacyRunWebVersion", &SHGInterface::LegacyRunWebVersion, "Runs a simulation from a configuration file to produce results for a website (legacy)")
       .property("number_of_segments", &SHGInterface::get_number_of_segments, &SHGInterface::set_number_of_segments,"Number of segments to use for simulation. -1 = auto, 1 = single, N = N segments")
       .property("num_threads", &SHGInterface::get_num_threads, &SHGInterface::set_num_threads, "Thread count: -1 = auto (all cores), 1 = single-threaded, N = N threads")
       .property("rng_strategy", &SHGInterface::get_rng_strategy, &SHGInterface::set_rng_strategy, "'RngStream' for MRG32k3a (default) or 'MersenneTwister' for Mersenne Twister")
       .property("cpd_format", &SHGInterface::get_cpd_format, &SHGInterface::set_cpd_format, "CPD output format: 'none' (fastest), 'sparse' (default, values only), 'legacy' (age (value) pairs for backwards compatibility)")
       .property("output_file", &SHGInterface::get_output_file, &SHGInterface::set_output_file, "Output file path. Empty = return DataFrame (default); set path = write CSV to disk like CLI")
       .property("input_data_folder", &SHGInterface::get_input_data_folder, &SHGInterface::set_input_data_folder, "Set or get the base folder for input data files. The individual file names are hardcoded for simplicity.")
       .property("immediate_cessation_year", &SHGInterface::get_immediate_cessation_year, &SHGInterface::set_immediate_cessation_year, "Set or get Immediate Cessation Year; If 0, no immediate cessation")
       .property("initiation_filename", &SHGInterface::get_initiation_filename, &SHGInterface::set_initiation_filename, "Set or get the initiation filename")
       .property("cessation_filename", &SHGInterface::get_cessation_filename, &SHGInterface::set_cessation_filename, "Set or get the cessation filename")
       .property("mortality_filename", &SHGInterface::get_mortality_filename, &SHGInterface::set_mortality_filename, "Set or get the mortality probabilities filename (e.g. acm.csv or ocm-excl-lung-cancer.csv)")
       .property("smok_params_source", &SHGInterface::get_smok_params_source, &SHGInterface::set_smok_params_source, "URL or local path of the last load_params() smoking zip (empty if unset)")
       .property("mort_params_source", &SHGInterface::get_mort_params_source, &SHGInterface::set_mort_params_source, "URL or local path of the last load_params() mortality zip (empty if unset)")
       .property("mort_params_type", &SHGInterface::get_mort_params_type, &SHGInterface::set_mort_params_type, "Mortality table from last load_params(): acm or ocm (empty if unset)")
       .property("params_cache_dir", &SHGInterface::get_params_cache_dir, "Read-only. Directory where load_params() stores extracted zip bundles (same as shg_params_cache_dir()). Delete this folder to clear the cache manually.")
       .property("cpd_filename", &SHGInterface::get_cpd_filename, &SHGInterface::set_cpd_filename, "Set or get the cpd filename")
       .property("mt_seeds", &SHGInterface::get_mt_seeds, &SHGInterface::set_mt_seeds, "Set or get MersenneTwister seeds. Must be a numeric vector of exactly 4 values (one for each stream: initiation, cessation, life table, individual). If not set, default seeds are used.")
       .property("rngstream_seed", &SHGInterface::get_rngstream_seed, &SHGInterface::set_rngstream_seed, "Set or get RngStream seed. Must be a numeric vector of exactly 6 values (a single seed array that generates 4 substreams, one for each stream: initiation, cessation, life table, individual). If not set, default seed is used.")
      .method("get_current_seeds", &SHGInterface::get_current_seeds, "Get the current seed(s) for the selected RNG strategy. Returns mt_seeds if rng_strategy is 'MersenneTwister', or rngstream_seed if rng_strategy is 'RngStream'. If no explicit seeds were set, returns the strategy defaults.")
      .method("reset_seeds_to_defaults", &SHGInterface::reset_seeds_to_defaults, "Reset the seed(s) to their default values for the currently selected RNG strategy. For MersenneTwister, sets mt_seeds to default values. For RngStream, sets rngstream_seed to default values.")
      .method("get_rng_state_fingerprint", &SHGInterface::get_rng_state_fingerprint, "Get a fingerprint of the RNG internal state. For RngStream, returns the actual internal state (24 values). For MersenneTwister, returns random numbers generated from each stream (12 values). Different seeds will produce different fingerprints, verifying that seeds are actually being used.")
      .method("get_data_shape", &SHGInterface::get_data_shape, "Get information about the shape/dimensions of the loaded input data. Returns a list with num_races, num_sexes, num_cohorts, age ranges, and CPD loading statistics.")
      .method("getConfig", (Rcpp::List (SHGInterface::*)()) &SHGInterface::getConfig, "Get current configuration as a list (same as getConfig(debug=FALSE)).")
      .method("getConfig", (Rcpp::List (SHGInterface::*)(bool)) &SHGInterface::getConfig, "Get current intent configuration as a list. Returns current settings including RNG strategy, seeds, input file paths, and simulation parameters as currently set on the instance. Set debug=TRUE for additional debug info.")
      .method("getReproConfig", (Rcpp::List (SHGInterface::*)()) &SHGInterface::getReproConfig, "Get reproducibility configuration from the last completed simulation (same as getReproConfig(debug=FALSE)).")
      .method("getReproConfig", (Rcpp::List (SHGInterface::*)(bool)) &SHGInterface::getReproConfig, "Get reproducibility configuration from the last completed simulation. Exports effective runtime segments/threads used by the run. Errors if no simulation has completed.")
      .method("useConfig", &SHGInterface::useConfig, "Apply configuration from a list. Sets all configuration parameters from a list previously obtained from getConfig() or manually created. Validates config version and warns about unknown fields.")
      .method("reset_to_factory_defaults", &SHGInterface::reset_to_factory_defaults,
              "Reset engine fields to the same defaults as a fresh SHGInterface() instance.")
      .method("get_shg_core_version", &SHGInterface::get_shg_core_version,
              "Compiled SHG core engine version string (from SHG_CORE_VERSION).")
      .method("last_completed_sim_was_fixed_cohort", &SHGInterface::last_completed_sim_was_fixed_cohort, "TRUE if the last completed simulation was runSimFromFixedValues (not a population runSimFromDataFrame). Portable save requires this to be TRUE.");
     // TODO: also antithetical variates; also increment substreams
  }


