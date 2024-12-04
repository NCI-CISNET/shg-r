// CISNET (www.cisnet.cancer.gov)
// Lung Cancer Base Case Group
// Rcpp wrapper for Smoking History Simulation Application
// Application to Simulate Initiation and Cessation Ages of individuals based on sex, race and year of birth.
// File: wrapper.cpp
// Author: John Clarke
// E-Mail: john.clarke@cornerstonenw.com
// NCI Contact: Rocky Feuer

// TODO: Update attribution and dates above

#include <cstdlib>    // General utilities
#include <cstring>    // C-style string functions
#include <iostream>   // Input/output stream objects
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
//' @title Test title SHGInterface
//' @aliases SHGInterface
//' @export
//' @description TEST Type the name of the class to see its methods
//' @field new Constructor
// class SHGInterface
// {
// public:
   // Eventually we should probably allow for a constructor that takes seeds
   SHGInterface::SHGInterface()
   {
      initialize();
   }

   Smoking_Simulator* SHGInterface::createSimulator()
   {
     const char *sInitiationProbFile = "./inst/inputs/2017-05-03/lbc_shg_initiation.txt";
     const char *sCessationProbFile = "./inst/inputs/2017-05-03/lbc_shg_cessation.txt";
     const char *sLifeTableFile = "./inst/inputs/2017-05-03/lbc_smokehist_oc_mortality.txt";
     const char *sCpdIntensityProbFile = ""; // no longer used?
     const char *sCpdDataFile = "./inst/inputs/2017-05-03/lbc_shg_cpd.txt";
     unsigned long ulInitPRNGSeed = 12345;
     unsigned long ulCessPRNGSeed = 12345;
     unsigned long ulLifeTabSeed = 12345;
     unsigned long ulIndivRndsSeed = 12345;
     short wOutputType = 2; // data only?
     short wCessationYear = 0;
     return new Smoking_Simulator(sInitiationProbFile, sCessationProbFile,
                           sLifeTableFile, sCpdIntensityProbFile,
                           sCpdDataFile, ulInitPRNGSeed,
                           ulCessPRNGSeed, ulLifeTabSeed,
                           ulIndivRndsSeed, wOutputType,
                           wCessationYear);
   }

   void SHGInterface::initialize()
   {
      // TODO: allow user to specify input files, seeds, etc.
      const char *sInitiationProbFile = "./inst/inputs/2017-05-03/lbc_shg_initiation.txt";
      const char *sCessationProbFile = "./inst/inputs/2017-05-03/lbc_shg_cessation.txt";
      const char *sLifeTableFile = "./inst/inputs/2017-05-03/lbc_smokehist_oc_mortality.txt";
      const char *sCpdIntensityProbFile = ""; // no longer used?
      const char *sCpdDataFile = "./inst/inputs/2017-05-03/lbc_shg_cpd.txt";
      unsigned long ulInitPRNGSeed = 12345;
      unsigned long ulCessPRNGSeed = 12345;
      unsigned long ulLifeTabSeed = 12345;
      unsigned long ulIndivRndsSeed = 12345;
      short wOutputType = 2; // data only?
      short wCessationYear = 0;

      pSimulator = new Smoking_Simulator(sInitiationProbFile, sCessationProbFile,
                                         sLifeTableFile, sCpdIntensityProbFile,
                                         sCpdDataFile, ulInitPRNGSeed,
                                         ulCessPRNGSeed, ulLifeTabSeed,
                                         ulIndivRndsSeed, wOutputType,
                                         wCessationYear);
   }
// race, sex, and cohort are fixed for this kind of simulation
void SHGInterface::runSimSegment(int repeat, short wRace, short wSex, short wYearBirth,
                                 std::vector<int>& initiationAge, std::vector<int>& cessationAge,
                                 std::vector<int>& ageAtDeath, std::vector<std::string>& cpdString,
                                 int offset) {
      //TODO we don't need an output file except to compare results with legacy code. Perhaps we can produce output only on demand?
      FILE *pOutStream = 0,
           *pErrorStream = 0;
      // TODO: creates an empty file, but we don't need it
      const char *sOutputFile = "./out/test_output_from_module2.txt";

      pOutStream = fopen(sOutputFile, "w");
      if (pOutStream == NULL)
      {
         fprintf(pErrorStream, "\n<ERROR>\nSupplied Output file: %s, could not be opened for writing.\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n", sOutputFile);
      }
      // TODO: Trying this to prevent an output file from being created
      pOutStream = NULL;

      string cpd;
      short wYearsAsSmoker, i;
      short sPersonsCPDbyAge;
      short sPersonsInitAge, sPersonsCessAge, sPersonsAgeAtDeath;

      Smoking_Simulator* qSimulator = createSimulator();

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

         // Print out the smoking intensity group for the person and the cigarettes smoked per day
         // Print the intensity group as +1 its value so range of values is from 1 to 5.
         // DRY violation -- this is also done in main.cpp but we don't copy those methods here.
         cpd = "";
         if (sPersonsInitAge != -999)
         {
            if (sPersonsCessAge == -999)
               wYearsAsSmoker = wSIM_CUTOFF_YEAR - (wDateBirths[k] + sPersonsInitAge) + 1;
            else
               wYearsAsSmoker = sPersonsCessAge - sPersonsInitAge + 1;
            for (i = 0; i < wYearsAsSmoker; i++)
            {
               if (i + sPersonsInitAge < 100)
               {
                  sPersonsCPDbyAge = dPersonsCPDbyAge[i];
                  cpd += std::to_string(i + sPersonsInitAge) + " (" + std::to_string(static_cast<int>(sPersonsCPDbyAge)) + "), ";
               }
            }
         }

         initiationAge[k] = qSimulator->GetPersonsInitAge();
         cessationAge[k] = qSimulator->GetPersonsCessAge();
         ageAtDeath[k] = qSimulator->GetPersonsAgeAtDeath();
         cpdString[k] = Rcpp::String(cpd);
      }
      fclose(pOutStream);
   }


// Run simulations in parallel OR sequentially and returned combine results
// The results should be identical regardless of the method used but assuming number_of_segments is the same
Rcpp::DataFrame SHGInterface::runSim(int repeat, short wRace, short wSex, short wYearBirth) {
   int n = number_of_segments; // Number of parallel simulations
   int repeat_per_sim = repeat / n;
   int remainder = repeat % n; // Calculate the remainder

   // Pre-allocate vectors
   std::vector<short> 
      initiationAge(repeat),
      cessationAge(repeat),
      ageAtDeath(repeat),
      wRaces(repeat, wRace),
      wSexes(repeat, wSex),
      wYearBirths(repeat, wYearBirth);
   std::vector<std::string> cpdString(repeat);
   std::vector<short> wRaces(repeat, wRace), wSexes(repeat, wSex);
   std::vector<int> wYearBirths(repeat, wYearBirth);

   // Vectors to store futures; declare even if we might not use it below
   std::vector<std::future<void>> futures;
   
   // Launch n simulations in parallel
   for (int i = 0; i < n; ++i) {
      int offset = i * repeat_per_sim;
      int current_repeat_per_sim = repeat_per_sim;

      // Add the remainder to the last segment
      if (i == n - 1) {
         current_repeat_per_sim += remainder;
      }

      if (run_multi_threaded) {
         // Run asynchronously across multiple threads
         futures.push_back(std::async(std::launch::async, &SHGInterface::runSimSegment, this,
                                     current_repeat_per_sim,
                                     std::ref(wRaces),
                                     std::ref(wSexes),
                                     std::ref(wYearBirths),
                                     std::ref(initiationAge),
                                     std::ref(cessationAge),
                                     std::ref(ageAtDeath),
                                     std::ref(cpdString),
                                     offset));
      }
      else {
         // Run sequentially using same segments
         SHGInterface::runSimSegment(current_repeat_per_sim,
                     std::ref(wRaces),
                     std::ref(wSexes),
                     std::ref(wYearBirths),
                     std::ref(initiationAge),
                     std::ref(cessationAge),
                     std::ref(ageAtDeath),
                     std::ref(cpdString),
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
      Rcpp::Named("wRace") = wRaceVec,
      Rcpp::Named("wSex") = wSexVec,
      Rcpp::Named("wYearBirth") = wYearBirthVec,
      Rcpp::Named("initiationAge") = initiationAgeVec,
      Rcpp::Named("cessationAge") = cessationAgeVec,
      Rcpp::Named("ageAtDeath") = ageAtDeathVec,
      Rcpp::Named("CPD") = cpdStringVec
   );

    return df;
}

   void SHGInterface::LegacyRunWebVersion(const char *sInputFileName)
   {
      RunWebVersion(sInputFileName);
      return;
   }

RCPP_MODULE(SmokingSimulator) {
   using namespace Rcpp;

// Rcpp_SHGInterface
//' Rcpp_SHGInterface Class
//' @name Rcpp_SHGInterface
//' @title Test title SHGInterface
//' @export
//' @description TEST Type the name of the class to see its methods
//' @field new Constructor
   class_<SHGInterface>("SHGInterface")
       .constructor()
       .method("runSim", &SHGInterface::runSim, "Generates a data frame of simulated smoking histories for n individuals")
       .method("LegacyRunWebVersion", &SHGInterface::LegacyRunWebVersion, "Runs a simulation from a configuration file to produce results for a website (legacy)")
       .property("number_of_segments", &SHGInterface::get_number_of_segments, &SHGInterface::set_number_of_segments,"Number of segments to use for single or multi-threaded simulation")
       .property("run_multi_threaded", &SHGInterface::get_run_multi_threaded, &SHGInterface::set_run_multi_threaded, "True if the simulation should be run asynchonously; False otherwise")
       .property("rng_strategy", &SHGInterface::get_rng_strategy, &SHGInterface::set_rng_strategy, "'RngStream' for MRG32k3a (default) or 'MersenneTwister' for Mersenne Twister");
      // TODO: allow user to specify the seed from R; also antithetical variates; also increment substreams
      // TODO: allow user to specify file or folder paths to input files
   }
