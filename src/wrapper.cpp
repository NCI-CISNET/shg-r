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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "wrapper.h"
#include "smoking_sim.h"
#include "sim_exception.h"
#include <Rcpp.h>

using namespace std;
      
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
//' @field number_of_segments Number of segments to use for single or multi-threaded simulation (default is 1). Note: MersenneTwister RNG is restricted to 1 segment. Use RngStream for multiple segments.
//' @field run_multi_threaded True if the simulation should be run asynchonously; False otherwise (default is False). Note: MersenneTwister RNG is restricted to non-parallel execution. Use RngStream for parallel execution. Also, parallel execution requires number_of_segments > 1.
//' @field rng_strategy 'RngStream' for MRG32k3a (default) or 'MersenneTwister' for Mersenne Twister RNG. 'RngStream' is recommended for reproducibility especially with multi-threaded simulations. Note: MersenneTwister RNG is restricted to single-segment, non-parallel execution due to limitations in maintaining IID properties across segments.
//' @field input_data_folder Set or get the base folder for input data files
//' @field initiation_filename Set or get the initiation filename
//' @field cessation_filename Set or get the cessation filename
//' @field lifetable_filename Set or get the lifetable filename
//' @field cpd_filename Set or get the cpd filename
//' @field immediate_cessation_year Set or get Immediate Cessation Year; If 0, no immediate cessation

SHGInterface::SHGInterface() {
   // For now there is no initialize needed;
   // The Smoking_Simulators are created on the fly
}

void SHGInterface::set_rng_strategy(string strategy) {
   if (strategy != "MersenneTwister" && strategy != "RngStream") {
      Rcpp::stop("Invalid RNG strategy. Must be 'RngStream' or 'MersenneTwister'");
   }
   
   // If switching to MersenneTwister, enforce restrictions
   if (strategy == "MersenneTwister") {
      if (number_of_segments > 1) {
         Rcpp::warning("Resetting number_of_segments to 1 for MersenneTwister RNG.");
         number_of_segments = 1;
      }
      if (run_multi_threaded) {
         Rcpp::warning("Resetting run_multi_threaded to FALSE for MersenneTwister RNG.");
         run_multi_threaded = false;
      }
   }
   
   rng_strategy = strategy;
}

void SHGInterface::set_number_of_segments(int n) {
   if (n < 1) {
      Rcpp::stop("number_of_segments must be >= 1");
   }
   
   if (rng_strategy == "MersenneTwister" && n > 1) {
      Rcpp::stop("MersenneTwister RNG cannot maintain IID properties with multiple segments. MersenneTwister is restricted to 1 segment. Use RngStream for multiple segments.");
   }
   
   number_of_segments = n;
}

void SHGInterface::set_run_multi_threaded(bool b) {
   if (rng_strategy == "MersenneTwister" && b) {
      Rcpp::stop("MersenneTwister RNG cannot maintain IID properties with parallel execution. MersenneTwister is restricted to non-parallel execution. Use RngStream for parallel execution.");
   }
   
   if (number_of_segments == 1 && b) {
      Rcpp::stop("run_multi_threaded cannot be TRUE when number_of_segments is 1. Parallel execution requires multiple segments.");
   }
   
   run_multi_threaded = b;
}

Smoking_Simulator* SHGInterface::loadSimulator()
{
   char *sInitiationFile = AssignFilename(input_data_folder.c_str(), initiation_filename.c_str());
   char *sCessationFile = AssignFilename(input_data_folder.c_str(), cessation_filename.c_str());
   char *sLifeTableFile = AssignFilename(input_data_folder.c_str(), lifetable_filename.c_str()); // oc_mortality or ac_mortality
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
//' @name runSimFromDataFrame
//' @title runSimFromDataFrame method
//' @description runSimFromDataFrame offers a way to configure and run a simulation from an existing R dataframe. It returns a dataframe of simulated smoking histories with the same number of rows and order as the input dataframe.
//' @param dfPopulation The input dataframe with named columns for race, sex, and birth_cohort
//' @examples
//' \dontrun{
//' library(SmokingHistoryGenerator)
//' shg <- new(SHGInterface)
//' shg$input_data_folder <- system.file("inputs/default", "", package="SmokingHistoryGenerator")
//' N <- 10^6
//' pop <- list(
//'     race = rep(0, N),
//'     sex = sample(x = c(0, 1), size = N, prob = c(0.5, 0.5), replace = TRUE),
//'     birth_cohort = rep(1930:1949, N / 20)
//' )
//' shg$rng_strategy <- "RngStream"
//' shg$number_of_segments <- 10 # if you have 10 cores
//' shg$run_multi_threaded <- TRUE
//' smoking_history <- shg$runSimFromDataFrame(pop)
//' }

Rcpp::DataFrame SHGInterface::runSimFromDataFrame(Rcpp::DataFrame dfPopulation) {

   if (!SHGInterface::isValidDataFrame(dfPopulation)) {
      Rcpp::stop("Invalid data frame");
   }
   
   // Validate RNG strategy restrictions
   if (rng_strategy == "MersenneTwister") {
      if (number_of_segments > 1) {
         Rcpp::stop("MersenneTwister RNG cannot maintain IID properties with multiple segments. MersenneTwister is restricted to 1 segment. Use RngStream for multiple segments.");
      }
      if (run_multi_threaded) {
         Rcpp::stop("MersenneTwister RNG cannot maintain IID properties with parallel execution. MersenneTwister is restricted to non-parallel execution. Use RngStream for parallel execution.");
      }
   }
   
   if (run_multi_threaded && number_of_segments == 1) {
      Rcpp::stop("run_multi_threaded cannot be TRUE when number_of_segments is 1. Parallel execution requires multiple segments.");
   }
   
   int repeat = dfPopulation.nrows();
   int n = number_of_segments; // Number of parallel simulations
   int repeat_per_sim = repeat / n;
   int remainder = repeat % n; // Calculate the remainder

   vector<short> 
      wRaces = Rcpp::as<vector<short>>(dfPopulation["race"]),
      wSexes = Rcpp::as<vector<short>>(dfPopulation["sex"]),
      wYearBirths = Rcpp::as<vector<short>>(dfPopulation["birth_cohort"]),
      initiationAge(repeat),
      cessationAge(repeat),
      ageAtDeath(repeat);
   vector<string> cpdString(repeat);

   // Vectors to store futures; declare even if we might not use it below
   vector<future<void>> futures;
   
   // Launch n simulations in parallel (or sequentially if run_multi_threaded is false)
   for (int i = 0; i < n; ++i) {
      int offset = i * repeat_per_sim;
      int current_repeat_per_sim = repeat_per_sim;

      // Add the remainder to the last segment
      if (i == n - 1) {
         current_repeat_per_sim += remainder;
      }

      if (run_multi_threaded) {
         // Run asynchronously across multiple threads
         futures.push_back(async(launch::async, &SHGInterface::runSimSegment, this,
                                     current_repeat_per_sim,
                                     ref(wRaces),
                                     ref(wSexes),
                                     ref(wYearBirths),
                                     ref(initiationAge),
                                     ref(cessationAge),
                                     ref(ageAtDeath),
                                     ref(cpdString),
                                     offset));
      }
      else {
         // Run sequentially using same segments
         SHGInterface::runSimSegment(current_repeat_per_sim,
                     ref(wRaces),
                     ref(wSexes),
                     ref(wYearBirths),
                     ref(initiationAge),
                     ref(cessationAge),
                     ref(ageAtDeath),
                     ref(cpdString),
                     offset);
      }
    }
    // Wait for all simulations to complete
    if (run_multi_threaded) {
      for (auto& fut : futures) {
        fut.get();
      }
    }

   // Convert to Rcpp::DataFrame
   Rcpp::IntegerVector wRaceVec(wRaces.begin(), wRaces.end());
   Rcpp::IntegerVector wSexVec(wSexes.begin(), wSexes.end());
   Rcpp::IntegerVector wYearBirthVec(wYearBirths.begin(), wYearBirths.end());
   Rcpp::IntegerVector initiationAgeVec(initiationAge.begin(), initiationAge.end());
   Rcpp::IntegerVector cessationAgeVec(cessationAge.begin(), cessationAge.end());
   Rcpp::IntegerVector ageAtDeathVec(ageAtDeath.begin(), ageAtDeath.end());
   Rcpp::CharacterVector cpdStringVec(cpdString.begin(), cpdString.end());

   Rcpp::DataFrame df = Rcpp::DataFrame::create(
      Rcpp::Named("race") = wRaceVec,
      Rcpp::Named("sex") = wSexVec,
      Rcpp::Named("birth_cohort") = wYearBirthVec,
      Rcpp::Named("smoking_initiation_age") = initiationAgeVec,
      Rcpp::Named("smoking_cessation_age") = cessationAgeVec,
      Rcpp::Named("age_at_death") = ageAtDeathVec,
      Rcpp::Named("cigarettes_per_day") = cpdStringVec
   );

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
//' shg$input_data_folder <- system.file("inputs/default", "", package="SmokingHistoryGenerator")
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
                              int offset) {

   // TODO we don't need an output file except to compare results with legacy code.
   FILE *pOutStream = NULL;

   string cpd;
   short wYearsAsSmoker;
   short sPersonsCPDbyAge;
   short sPersonsInitAge, sPersonsCessAge, sPersonsAgeAtDeath;

   // For now we instantiate a different simulator for each segment rather than loading the input data and cloning
   // Otherwise, we get errors probably to do with memory sharing; could investigate further
   Smoking_Simulator* qSimulator = loadSimulator();

   // TODO: allow user to specify the seed from R
   if (rng_strategy == "MersenneTwister")
      qSimulator->setRNGStrategy(new MersenneTwisterRNG(1898587603, 1468371936, 1551308340, 1590227640));
   else if (rng_strategy == "RngStream")
      qSimulator->setRNGStrategy(new RngStreamRNG());
   else
      Rcpp::stop("Invalid RNG strategy or strategy not yet implemented");

   // TODO: review the following; not sure this is the best pattern
   // We could include another parameter in the function signature to pass the segment number;
   // But this works also. The idea is ensure that the RNG state is advanced in the same way for each segment so that the results are identical and IID
   int segment_number = offset / repeat; // expected 0, 1, 2... for each segment
   qSimulator->incrementSubstreams(segment_number);

   for (int j = 0; j < repeat; j++)
   {
      int k = offset + j;
      qSimulator->RunSimulationSingle(wRaces[k], wSexes[k], wDateBirths[k], pOutStream);

      double* dPersonsCPDbyAge = qSimulator->GetPersonsCPDbyAge();
      sPersonsInitAge = qSimulator->GetPersonsInitAge();
      sPersonsCessAge = qSimulator->GetPersonsCessAge();
      sPersonsAgeAtDeath = qSimulator->GetPersonsAgeAtDeath();

      // Get the smoking intensity group for the person and the cigarettes smoked per day
      // The intensity group as +1 its value so range of values is from 1 to 5.
      // DRY violation -- this is also done in main.cpp but we don't copy those methods here.
      cpd = "";
      if (sPersonsInitAge != -999)
      {
         if (sPersonsCessAge == -999)
            wYearsAsSmoker = wSIM_CUTOFF_YEAR - (wDateBirths[k] + sPersonsInitAge) + 1;
         else
            wYearsAsSmoker = sPersonsCessAge - sPersonsInitAge + 1;
         for (int i = 0; i < wYearsAsSmoker; i++)
         {
            if (i + sPersonsInitAge < 100)
            {
               sPersonsCPDbyAge = dPersonsCPDbyAge[i];
               cpd += to_string(i + sPersonsInitAge) + " (" + to_string(static_cast<int>(sPersonsCPDbyAge)) + "), ";
            }
         }
      }

      initiationAge[k] = sPersonsInitAge;
      cessationAge[k] = sPersonsCessAge;
      ageAtDeath[k] = sPersonsAgeAtDeath;
      cpdString[k] = Rcpp::String(cpd);
   }
   // fclose(pOutStream); # this caused a segfault in Ubuntu and is probably not needed because there is no output file for the Rcpp version
}

bool SHGInterface::fileExists(const char* filename) {
   ifstream file(filename);
   return file.good();
}
//' @name LegacyRunWebVersion
//' @title LegacyRunWebVersion method
//' @description This method offers a way to configure and run a simulation from an input configuration file. Rather than return a R DataFrame, it produces results in an output file. It works in the same as calling the CLI version of the Smoking History Generator with a single input file parameter.
//' @param input_file_name The name of the configuration file (see ./inst/inputs/ for 2 examples)
//' @examples
//' \dontrun{
//' # Warning: This way of running a simulation ignores the Rcpp interface properties and relies soley 
//' # on parameters set in the input configuration file. See main.cpp's RunWebVersion for more detail.
//' library(SmokingHistoryGenerator)
//' shg <- new(SHGInterface)
//' shg$input_data_folder <- system.file("inputs/default", "", package="SmokingHistoryGenerator")
//' example_input_filepath <- system.file("inputs/examples/", "test_input_example_MersenneTwister.txt", package="SmokingHistoryGenerator")
//' shg$LegacyRunWebVersion(example_input_filepath)
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

RCPP_MODULE(SmokingSimulator) {
   using namespace Rcpp;

// Rcpp_SHGInterface
//' Rcpp SHG Interface Class
//' @name Rcpp_SHGInterface
//' @title Rcpp SHG Interface Class
//' @export
//' @description This module provides an Rcpp interface to the Smoking History Generator (SHG) application.
   class_<SHGInterface>("SHGInterface")
       .constructor()
       .method("runSimFromFixedValues", &SHGInterface::runSimFromFixedValues, "Generates a data frame of simulated smoking histories for n individuals")
       .method("runSimFromDataFrame", &SHGInterface::runSimFromDataFrame, "Generates a data frame of simulated smoking histories for n individuals")
       .method("LegacyRunWebVersion", &SHGInterface::LegacyRunWebVersion, "Runs a simulation from a configuration file to produce results for a website (legacy)")
       .property("number_of_segments", &SHGInterface::get_number_of_segments, &SHGInterface::set_number_of_segments,"Number of segments to use for single or multi-threaded simulation")
       .property("run_multi_threaded", &SHGInterface::get_run_multi_threaded, &SHGInterface::set_run_multi_threaded, "True if the simulation should be run asynchonously; False otherwise")
       .property("rng_strategy", &SHGInterface::get_rng_strategy, &SHGInterface::set_rng_strategy, "'RngStream' for MRG32k3a (default) or 'MersenneTwister' for Mersenne Twister")
       .property("input_data_folder", &SHGInterface::get_input_data_folder, &SHGInterface::set_input_data_folder, "Set or get the base folder for input data files. The individual file names are hardcoded for simplicity.")
       .property("immediate_cessation_year", &SHGInterface::get_immediate_cessation_year, &SHGInterface::set_immediate_cessation_year, "Set or get Immediate Cessation Year; If 0, no immediate cessation")
       .property("initiation_filename", &SHGInterface::get_initiation_filename, &SHGInterface::set_initiation_filename, "Set or get the initiation filename")
       .property("cessation_filename", &SHGInterface::get_cessation_filename, &SHGInterface::set_cessation_filename, "Set or get the cessation filename")
       .property("lifetable_filename", &SHGInterface::get_lifetable_filename, &SHGInterface::set_lifetable_filename, "Set or get the lifetable filename")
       .property("cpd_filename", &SHGInterface::get_cpd_filename, &SHGInterface::set_cpd_filename, "Set or get the cpd filename");
      // TODO: allow user to specify the seed from R; also antithetical variates; also increment substreams
   }

