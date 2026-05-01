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
//' @field lifetable_filename Set or get the mortality input filename (legacy name; same as mortality_filename)
//' @field mortality_filename Set or get the mortality probabilities filename (e.g. acm.csv or ocm-excl-lung-cancer.csv)
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
      useConfig(config);
   }
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
      return get_mt_seeds();
   } else if (rng_strategy == "RngStream") {
      return get_rngstream_seed();
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
   Smoking_Simulator* qSimulator = loadSimulator();
   
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
      delete qSimulator;
      Rcpp::stop("Invalid RNG strategy. Cannot get fingerprint for strategy: " + rng_strategy);
   }
   
   // Get the fingerprint from the RNG strategy
   std::vector<double> fingerprint = qSimulator->getRNGStateFingerprint();
   
   // Convert to Rcpp::NumericVector
   Rcpp::NumericVector result(fingerprint.size());
   for (size_t i = 0; i < fingerprint.size(); i++) {
      result[i] = fingerprint[i];
   }
   
   delete qSimulator;
   return result;
}

Smoking_Simulator* SHGInterface::loadSimulator()
{
   char *sInitiationFile = AssignFilename(input_data_folder.c_str(), initiation_filename.c_str());
   char *sCessationFile = AssignFilename(input_data_folder.c_str(), cessation_filename.c_str());
   char *sLifeTableFile = AssignFilename(input_data_folder.c_str(), lifetable_filename.c_str()); // OCM or ACM mortality table
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
//' @description Returns a list containing information about the shape/dimensions of the loaded input data.
//'              This is populated after a simulation is run and shows the structure of the parameter files.
//' @return A list with data shape information including races, sexes, cohorts, age ranges, and CPD statistics.
Rcpp::List SHGInterface::get_data_shape() {
   return Rcpp::List::create(
      Rcpp::Named("num_races") = last_num_races,
      Rcpp::Named("num_sexes") = last_num_sexes,
      Rcpp::Named("num_cohorts") = last_num_cohorts,
      Rcpp::Named("first_cohort") = Rcpp::IntegerVector::create(
         Rcpp::Named("start") = last_first_cohort_start,
         Rcpp::Named("end") = last_first_cohort_end
      ),
      Rcpp::Named("last_cohort") = Rcpp::IntegerVector::create(
         Rcpp::Named("start") = last_last_cohort_start,
         Rcpp::Named("end") = last_last_cohort_end
      ),
      Rcpp::Named("initiation_ages") = Rcpp::IntegerVector::create(
         Rcpp::Named("min") = last_min_init_age,
         Rcpp::Named("max") = last_max_init_age
      ),
      Rcpp::Named("cessation_ages") = Rcpp::IntegerVector::create(
         Rcpp::Named("min") = last_min_cess_age,
         Rcpp::Named("max") = last_max_cess_age
      ),
      Rcpp::Named("cpd_ages") = Rcpp::IntegerVector::create(
         Rcpp::Named("min") = (int)last_cpd_min_age,
         Rcpp::Named("max") = (int)last_cpd_max_age
      ),
      Rcpp::Named("num_intensity_groups") = last_num_intensity_grps,
      Rcpp::Named("cpd_rows_loaded") = (int)last_cpd_rows_loaded,
      Rcpp::Named("cpd_rows_skipped") = (int)last_cpd_rows_skipped
   );
}

//' @name runSimFromDataFrame
//' @title runSimFromDataFrame method
//' @description runSimFromDataFrame offers a way to configure and run a simulation from an existing R dataframe. It returns a dataframe of simulated smoking histories with the same number of rows and order as the input dataframe.
//' @details On Windows, \code{output_file} (direct disk output) cannot be combined with
//'          multi-threaded execution (\code{num_threads} not equal to \code{1}). The call stops with an error
//'          before loading inputs or writing files. Use the default in-memory DataFrame return value, or set
//'          \code{num_threads <- 1} to write a file.
//' @param dfPopulation The input dataframe with named columns for race, sex, and birth_cohort
//' @examples
//' \dontrun{
//' library(SmokingHistoryGenerator)
//' shg <- new(SHGInterface)
//' # Multi-cohort populations need full NHIS-style inputs (all cohort columns).
//' # inst/extdata is CRAN-sized only; full bundle coming soon on Zenodo — see README.
//' shg$input_data_folder <- "/path/to/NHIS-1965-2016/csv-complete"
//' N <- 10^6
//' pop <- list(
//'     race = rep(0, N),
//'     sex = sample(x = c(0, 1), size = N, prob = c(0.5, 0.5), replace = TRUE),
//'     birth_cohort = rep(1930:1949, N / 20)
//' )
//' shg$rng_strategy <- "RngStream"
//' # Optionally set a custom seed for RngStream (6 values)
//' shg$rngstream_seed <- c(12345, 12345, 12345, 12345, 12345, 12345)
//' shg$number_of_segments <- -1 # auto-calculate (default), or set explicit value for reproducibility
//' shg$num_threads <- -1  # -1 = auto (all cores), 1 = single-threaded
//' smoking_history <- shg$runSimFromDataFrame(pop)
//' 
//' # Example with MersenneTwister and custom seeds (4 values)
//' shg2 <- new(SHGInterface)
//' shg2$input_data_folder <- system.file("extdata", package="SmokingHistoryGenerator")
//' shg2$rng_strategy <- "MersenneTwister"
//' shg2$mt_seeds <- c(1898587603, 1468371936, 1551308340, 1590227640)
//' smoking_history2 <- shg2$runSimFromFixedValues(1000, 0, 0, 1950)
//' }

Rcpp::DataFrame SHGInterface::runSimFromDataFrame(Rcpp::DataFrame dfPopulation) {

   if (!SHGInterface::isValidDataFrame(dfPopulation)) {
      Rcpp::stop("Invalid data frame");
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
   int repeat_per_sim = repeat / n;
   int remainder = repeat % n; // Calculate the remainder

   vector<short> 
      wRaces = Rcpp::as<vector<short>>(dfPopulation["race"]),
      wSexes = Rcpp::as<vector<short>>(dfPopulation["sex"]),
      wYearBirths = Rcpp::as<vector<short>>(dfPopulation["birth_cohort"]);

   // Create shared data once for all segments (major performance optimization)
   string initFile = input_data_folder + "/" + initiation_filename;
   string cessFile = input_data_folder + "/" + cessation_filename;
   string lifeFile = input_data_folder + "/" + lifetable_filename;
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
      
      for (int seg = 0; seg < n; seg++) {
         int offset = seg * repeat_per_sim;
         int current_repeat = repeat_per_sim + (seg == n - 1 ? remainder : 0);
         
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
      
      // Return minimal DataFrame with info
      return Rcpp::DataFrame::create(
         Rcpp::Named("info") = Rcpp::CharacterVector::create("Results written to file: " + output_file),
         Rcpp::Named("rows") = Rcpp::IntegerVector::create(repeat)
      );
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
   for (int i = 0; i < n; ++i) {
      int offset = i * repeat_per_sim;
      int current_repeat_per_sim = repeat_per_sim;

      // Add the remainder to the last segment
      if (i == n - 1) {
         current_repeat_per_sim += remainder;
      }

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

    return df;
}
//' @name runSimFromFixedValues
//' @title runSimFromFixedValues method
//' @description runSimFromFixedValues offers a way to configure and run a simulation from fixed values for race, sex, and birth year cohort rather than passing a data frame. It returns a dataframe of simulated smoking histories for n individuals.
//' @param repeat The number of individuals to simulate
//' @param race (default = 0 and refers to all races combined)
//' @param sex (0 for male, 1, for female)
//' @param cohort_year (four digit birth cohort year)
//' @examples
//' \dontrun{
//' library(SmokingHistoryGenerator)
//' shg <- new(SHGInterface)
//' shg$input_data_folder <- system.file("extdata", package="SmokingHistoryGenerator")
//' N <- 10^6
//' smoking_history <- shg$runSimFromFixedValues(N, 0, 0, 1950)
//' }
Rcpp::DataFrame SHGInterface::runSimFromFixedValues(int repeat, short wRace, short wSex, short wYearBirth) {

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

   Rcpp::DataFrame result = runSimFromDataFrame(df);
   return result;
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
      if (raceVec[i] != 0 && raceVec[i] != 1) {
            Rcpp::stop("Invalid value of '" + to_string(raceVec[i]) + "' for race at index " + to_string(i));
      }
   }

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

   // Set RNG strategy with user-specified seeds or defaults
   // Skip oversampling for RngStream (performance), keep for MT (backwards compatibility)
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
      qSimulator->gbSkipOversampling = true;   // RngStream: skip oversampling (faster, no benefit)
      
      // Create base RNG
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
      
      // Wrap with buffering for 13-15% performance improvement (same as CLI)
      // Buffer size 10000 matches CLI (reduces RNG function call overhead)
      BufferedRngStreamRNG* bufferedRng = new BufferedRngStreamRNG(baseRng, 10000, true);
      qSimulator->setRNGStrategy(bufferedRng);
   }
   else
      Rcpp::stop("Invalid RNG strategy or strategy not yet implemented");

   // TODO: review the following; not sure this is the best pattern
   // We could include another parameter in the function signature to pass the segment number;
   // But this works also. The idea is ensure that the RNG state is advanced in the same way for each segment so that the results are identical and IID
   int segment_number = offset / repeat; // expected 0, 1, 2... for each segment
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
   
   // Set RNG strategy
   // Skip oversampling for RngStream (performance), keep for MT (backwards compatibility)
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
      qSimulator->gbSkipOversampling = true;   // RngStream: skip oversampling (faster, no benefit)
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
   string lifeFile = input_data_folder + "/" + lifetable_filename;
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
//' @param input_file_name Path to a Legacy web-style configuration file. Paths inside the file are resolved relative to the R process working directory (the \code{input_data_folder} property is ignored). Sample text configs live under \code{tests/testdata/legacy-web-examples/} in the package source; for installed use, build a config with absolute paths from \code{system.file("extdata", package = "SmokingHistoryGenerator")}.
//' @examples
//' \dontrun{
//' # Warning: LegacyRunWebVersion ignores Rcpp properties and uses only the config file.
//' library(SmokingHistoryGenerator)
//' shg <- new(SHGInterface)
//' d <- system.file("extdata", package = "SmokingHistoryGenerator")
//' tf <- tempfile(fileext = ".txt")
//' writeLines(c(
//'   "RNGSTRATEGY=RngStream",
//'   "RNGSTREAM_SEED=12345,12345,12345,12345,12345,12345",
//'   "RACE=0", "SEX=0", "YOB=1950", "CESSATION_YR=0", "REPEAT=100",
//'   paste0("INIT_PROB=", file.path(d, "initiation.csv")),
//'   paste0("CESS_PROB=", file.path(d, "cessation.csv")),
//'   paste0("MORTALITY_PROB=", file.path(d, "acm.csv")),
//'   paste0("CPD_DATA=", file.path(d, "cpd.csv")),
//'   paste0("OUTPUTFILE=", tempfile("out_", fileext = ".txt")),
//'   paste0("ERRORFILE=", tempfile("err_", fileext = ".txt"))
//' ), tf)
//' shg$LegacyRunWebVersion(tf)
//' }
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
//' @return A list containing the current configuration including: config_version, rng_strategy, number_of_segments, num_threads, seeds, input file paths, immediate_cessation_year, and timestamp. If debug=TRUE, also includes rng_state_fingerprint, package_version, package_source, r_version, platform, and memory_usage.
//' @examples
//' \dontrun{
//' library(SmokingHistoryGenerator)
//' shg <- new(SHGInterface)
//' shg$rng_strategy <- "RngStream"
//' shg$number_of_segments <- 4
//' config <- shg$getConfig()
//' # Save config for later use
//' saveRDS(config, "my_config.rds")
//' # Get config with debug info
//' debug_config <- shg$getConfig(debug = TRUE)
//' }
Rcpp::List SHGInterface::getConfig(bool debug) {
   Rcpp::List config;
   
   // Config version for future compatibility
   config["config_version"] = "1.0";
   
  // Basic configuration
  config["rng_strategy"] = rng_strategy;
  config["number_of_segments"] = number_of_segments;
  config["num_threads"] = num_threads;
  
  // Get seeds using get_current_seeds()
  Rcpp::NumericVector seeds = get_current_seeds();
   config["seeds"] = seeds;
   
   // Input file configuration
   config["input_data_folder"] = input_data_folder;
   config["initiation_filename"] = initiation_filename;
   config["cessation_filename"] = cessation_filename;
   config["lifetable_filename"] = lifetable_filename;
   config["mortality_filename"] = lifetable_filename;
   config["cpd_filename"] = cpd_filename;
   config["immediate_cessation_year"] = immediate_cessation_year;
   
   // Timestamp
   Rcpp::Function Sys_time("Sys.time");
   Rcpp::Function format("format");
   Rcpp::RObject time_obj = Sys_time();
   Rcpp::String timestamp = Rcpp::as<std::string>(format(time_obj, Rcpp::_["format"] = "%Y-%m-%d %H:%M:%S"));
   config["timestamp"] = timestamp;
   
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

// Wrapper method without debug parameter (defaults to false)
Rcpp::List SHGInterface::getConfig() {
   return getConfig(false);
}

//' Configure SHG instance from config object
//' @name useConfig
//' @title Use SHG Configuration
//' @description Configures an existing SHG instance from a configuration object (typically obtained from getConfig()).
//' @param config A list containing configuration parameters. Must include config_version. All parameters are validated.
//' @details This method validates the config_version and all parameters before setting them. Unknown fields are warned about but allowed for future compatibility. Missing optional fields use defaults. Fields are applied in an order suitable for round-trips from getConfig(): number_of_segments and num_threads are set before rng_strategy (so switching to Mersenne Twister does not message when the saved list already has single-threaded settings), then seeds, then paths and other options. If the list has deprecated \code{run_multi_threaded} but no \code{num_threads}, it is mapped: FALSE -> \code{num_threads = 1}, TRUE -> \code{num_threads = -1}. If both are present, \code{num_threads} wins.
//' @examples
//' \dontrun{
//' library(SmokingHistoryGenerator)
//' # Create and configure first instance
//' shg1 <- new(SHGInterface)
//' shg1$rng_strategy <- "RngStream"
//' shg1$number_of_segments <- 4
//' config <- shg1$getConfig()
//' 
//' # Create new instance and apply config
//' shg2 <- new(SHGInterface)
//' shg2$useConfig(config)
//' # shg2 now has same configuration as shg1
//' }
void SHGInterface::useConfig(Rcpp::List config) {
   // Validate config_version
   if (!config.containsElementNamed("config_version")) {
      Rcpp::warning("Config missing config_version field. Assuming version 1.0.");
   } else {
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
   
   if (config.containsElementNamed("initiation_filename")) {
      set_initiation_filename(Rcpp::as<std::string>(config["initiation_filename"]));
   }
   
   if (config.containsElementNamed("cessation_filename")) {
      set_cessation_filename(Rcpp::as<std::string>(config["cessation_filename"]));
   }
   
   if (config.containsElementNamed("mortality_filename")) {
      set_mortality_filename(Rcpp::as<std::string>(config["mortality_filename"]));
   } else if (config.containsElementNamed("lifetable_filename")) {
      set_lifetable_filename(Rcpp::as<std::string>(config["lifetable_filename"]));
   }
   
   if (config.containsElementNamed("cpd_filename")) {
      set_cpd_filename(Rcpp::as<std::string>(config["cpd_filename"]));
   }
   
  if (config.containsElementNamed("immediate_cessation_year")) {
      set_immediate_cessation_year(Rcpp::as<int>(config["immediate_cessation_year"]));
   }
  
  // Warn about unknown fields (but allow for future compatibility)
  std::vector<std::string> known_fields = {
    "config_version", "rng_strategy", "number_of_segments", "num_threads", "run_multi_threaded",
    "seeds", "input_data_folder", "initiation_filename", "cessation_filename",
    "lifetable_filename", "mortality_filename", "cpd_filename", "immediate_cessation_year", "timestamp",
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
//' @description This module provides an Rcpp interface to the Smoking History Generator (SHG) application.
   class_<SHGInterface>("SHGInterface")
       .constructor("Create a new SHGInterface instance")
       .constructor<Rcpp::List>("Create a new SHGInterface instance with optional config parameter")
       .method("runSimFromFixedValues", &SHGInterface::runSimFromFixedValues, "Generates a data frame of simulated smoking histories for n individuals")
       .method("runSimFromDataFrame", &SHGInterface::runSimFromDataFrame, "Generates a data frame of simulated smoking histories for n individuals")
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
       .property("lifetable_filename", &SHGInterface::get_lifetable_filename, &SHGInterface::set_lifetable_filename, "Set or get the mortality input filename (legacy name; prefer mortality_filename)")
       .property("mortality_filename", &SHGInterface::get_mortality_filename, &SHGInterface::set_mortality_filename, "Set or get the mortality probabilities filename (e.g. acm.csv or ocm-excl-lung-cancer.csv)")
       .property("cpd_filename", &SHGInterface::get_cpd_filename, &SHGInterface::set_cpd_filename, "Set or get the cpd filename")
       .property("mt_seeds", &SHGInterface::get_mt_seeds, &SHGInterface::set_mt_seeds, "Set or get MersenneTwister seeds. Must be a numeric vector of exactly 4 values (one for each stream: initiation, cessation, life table, individual). If not set, default seeds are used.")
       .property("rngstream_seed", &SHGInterface::get_rngstream_seed, &SHGInterface::set_rngstream_seed, "Set or get RngStream seed. Must be a numeric vector of exactly 6 values (a single seed array that generates 4 substreams, one for each stream: initiation, cessation, life table, individual). If not set, default seed is used.")
      .method("get_current_seeds", &SHGInterface::get_current_seeds, "Get the current seed(s) for the selected RNG strategy. Returns mt_seeds if rng_strategy is 'MersenneTwister', or rngstream_seed if rng_strategy is 'RngStream'. Returns empty vector if seeds have not been explicitly set (defaults will be used).")
      .method("reset_seeds_to_defaults", &SHGInterface::reset_seeds_to_defaults, "Reset the seed(s) to their default values for the currently selected RNG strategy. For MersenneTwister, sets mt_seeds to default values. For RngStream, sets rngstream_seed to default values.")
      .method("get_rng_state_fingerprint", &SHGInterface::get_rng_state_fingerprint, "Get a fingerprint of the RNG internal state. For RngStream, returns the actual internal state (24 values). For MersenneTwister, returns random numbers generated from each stream (12 values). Different seeds will produce different fingerprints, verifying that seeds are actually being used.")
      .method("get_data_shape", &SHGInterface::get_data_shape, "Get information about the shape/dimensions of the loaded input data. Returns a list with num_races, num_sexes, num_cohorts, age ranges, and CPD loading statistics.")
      .method("getConfig", (Rcpp::List (SHGInterface::*)(bool)) &SHGInterface::getConfig, "Get current configuration as a list. Returns all current settings including RNG strategy, seeds, input file paths, and simulation parameters. Set debug=TRUE for additional debug info.")
      .method("useConfig", &SHGInterface::useConfig, "Apply configuration from a list. Sets all configuration parameters from a list previously obtained from getConfig() or manually created. Validates config version and warns about unknown fields.");
     // TODO: also antithetical variates; also increment substreams
  }


