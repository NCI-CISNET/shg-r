// CISNET (www.cisnet.cancer.gov)
// Lung Cancer Base Case Group
// Rcpp wrapper for Smoking History Simulation Application
// Application to Simulate Initiation and Cessation Ages of individuals based on sex, race and year of birth.
// File: wrapper.cpp
// Author: John Clarke
// E-Mail: john.clarke@cornerstonenw.com
// NCI Contact: Rocky Feuer

// TODO: Update attribution and dates above

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <string>
#include <stdlib.h>
#include <limits>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>

#include <string>
#include <sstream>
#include <vector>
#include <iterator>
#include <iostream>
#include <algorithm>
#include <future>
#include <thread>

#include "wrapper.h"
#include "smoking_sim.h"
#include "sim_exception.h"

#define MAX(x) (std::numeric_limits<x>::max())

#define DEFAULT_DATA_DIR "data/nhis_inputs_jan_2009/"
#define COUNTERFACTUAL_DATA_DIR "data/counterfactual_inputs_jan_2009/"

// Input file names
#define INITIATION_DATA_FILE "lbc_smokehist_initiation.txt"
#define CESSATION_DATA_FILE "lbc_smokehist_cessation.txt"
#define OTHER_COD_DATA_FILE "lbc_smokehist_oc_mortality.txt"
#define CPD_INTENSITY_PROBS "lbc_smokehist_cpdintensityprobs.txt"
#define CPD_DATA_FILE "lbc_smokehist_cpd.txt"

#define VECTOR_DELIMITER ","
#define MAX_NUM_REPS 1000000
#define VERSION_FILE "version.json"

#define E_S ""

using namespace std;

//std::string VERSIONJSON;
//std::string VERSION_NUM;
//std::string gInputFileName;

////bool gWithHoldTags = false;

////const short wMIN_IMMEDIATE_CESSATION_YEAR = 1910;  // Minimum Year Value that can be used as the Immediatte Cessation Year
////short wSIM_CUTOFF_YEAR = 2200;                     // Cut-off year for the application

////const char sSEX_LABELS[2][7]  = {"Male", "Female"};
////const char sRACE_LABELS[2][10] = {"All Races", "White"};

// Declaring Function prototypes
//char* AssignFilename(const char* sDirectory, const char * sFilename);
////short CountVectorValues(char* sDataString);
//bool CreateDataFile(const char *sNumToSimulate, const char* sOutFileName, char*);
//void Help(const char* sAppName, FILE* pHelpStream);
////bool IsPosLongInt(const char *sValue);
//bool IsPosShortInt(const char *sValue);
 
////bool IsValidNumReps(const char* sNumReps);

// TODO: do we need this anymore, given that the strategy should probably do the checking?
bool IsValidSeed(const char* sSeedValue);

void LoadValue(char* sDest, char* sSource, int iValueNum);
//void ModifyCutoffYear(char*);
//bool RunFromParameters(const char*, char*, char*, char*, char*, char*, char*, char*, char*, char*);
//void RunInfiniteLoop();
//void RunInterface();
int RunWebVersion(const char *sInputFileName);
char* Str_toupper(char *s);
//char* Str_tolower(char *s);
//void Usage();
short min(short, short);
// bool ValidateParameters(char*, char*, char*, char*, char*, char*, char*, char*, char*);
// bool ValidateParameters(char*, char*, char*, char*, char*, char*, char*, char*, char*, char*);

////void WriteInputTag(FILE* , char*, char*, const char*, const char*);
// void WriteRunInfoTag(FILE* , std::string, const char*, const char*, const char*,
//                      const char*, const char*, const char*, const char*, const char*,
//                      const char*, const char*, const char*, const char*);
//void PropagateVersionInformation();

short wRace = 0;
   short wSex = 0;
   short wYearBirth = 1950;


#define STRICT_R_HEADERS
#include <Rcpp.h>
// [[Rcpp::depends(Rcpp)]]

using namespace Rcpp;
#define BUFSZ 512

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
      const char *sOutputFile = "./out/test_output_from_module2.txt";

      pOutStream = fopen(sOutputFile, "w");
      if (pOutStream == NULL)
      {
         fprintf(pErrorStream, "\n<ERROR>\nSupplied Output file: %s, could not be opened for writing.\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n", sOutputFile);
      }
      // Trying this to prevent an output file from being created
      pOutStream = NULL;

      string cpd;
      short wYearsAsSmoker, i;
      short sPersonsCPDbyAge;
      short sPersonsInitAge, sPersonsCessAge, sPersonsAgeAtDeath;

      Smoking_Simulator* qSimulator = createSimulator();

      for (int j = 0; j < repeat; j++)
      {
         qSimulator->RunSimulationSingle(wRace, wSex, wYearBirth, pOutStream);

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
               wYearsAsSmoker = wSIM_CUTOFF_YEAR - (wYearBirth + sPersonsInitAge) + 1;
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

         initiationAge[offset + j] = qSimulator->GetPersonsInitAge();
         cessationAge[offset + j] = qSimulator->GetPersonsCessAge();
         ageAtDeath[offset + j] = qSimulator->GetPersonsAgeAtDeath();
         cpdString[offset + j] = Rcpp::String(cpd);
      
   }
      fclose(pOutStream);
   }

//Initialize with 
// RNGtype 
// SEED_INIT=12345
// SEED_CESS=12345
// SEED_OCD=12345
// SEED_MISC=12345
// RACE=0
// SEX=0
// YOB=1950
// CESSATION_YEAR=0
// REPEAT=1000
// INIT_PROB=./inst/inputs/2017-05-03/lbc_shg_initiation.txt
// CESS_PROB=./inst/inputs/2017-05-03/lbc_shg_cessation.txt
// OCD_PROB=./inst/inputs/2017-05-03/lbc_smokehist_oc_mortality.txt
// CPD_DATA=./inst/inputs/2017-05-03/lbc_shg_cpd.txt
// OUTPUTFILE=./out/test_output.out
// ERRORFILE=./out/test_errors.txt

//SHGInterface::runSim
// specify N
// Rcpp::DataFrame SHGInterface::runSim(int repeat)
// {
//    short wRace = 0;
//    short wSex = 0;
//    short wYearBirth = 1950;
//    return runSim(repeat, wRace, wSex, wYearBirth);
// }

// runSim( with pop matrix already set up (random race, sex etc.))
// runSingleSim so from R for individual enquiries
// ResetRNGs
// ChooseRNGs
// set the input files
// max/min cessation year
// output to file as well (for legacy)
// set the seeds for either RNG

// add tests (use sample output from legacy code)
// t-test
// runSimFromInputFile() need to expose this



// Function to run simulations in parallel and combine results
Rcpp::DataFrame SHGInterface::runSim(int repeat, short wRace, short wSex, short wYearBirth) {
    int n = 10; // Number of parallel simulations
    int repeat_per_sim = repeat / n;
    int remainder = repeat % n; // Calculate the remainder

    // Pre-allocate vectors
    std::vector<int> initiationAge(repeat), cessationAge(repeat), ageAtDeath(repeat);
    std::vector<std::string> cpdString(repeat);
    std::vector<short> wRaces(repeat, wRace), wSexes(repeat, wSex);
    std::vector<int> wYearBirths(repeat, wYearBirth);

    // Vectors to store futures
    std::vector<std::future<void>> futures;

    // Launch n simulations in parallel
    for (int i = 0; i < n; ++i) {
        int offset = i * repeat_per_sim;
        int current_repeat_per_sim = repeat_per_sim;

        // Add the remainder to the last segment
        if (i == n - 1) {
            current_repeat_per_sim += remainder;
        }

        futures.push_back(std::async(std::launch::async, &SHGInterface::runSimSegment, this,
                                     current_repeat_per_sim, wRace, wSex, wYearBirth,
                                     std::ref(initiationAge), std::ref(cessationAge),
                                     std::ref(ageAtDeath), std::ref(cpdString), offset));
    }

    // Wait for all simulations to complete
    for (auto& fut : futures) {
        fut.get();
    }

    // Convert to Rcpp::DataFrame
    return Rcpp::DataFrame::create(
        Rcpp::Named("wRace") = wRaces,
        Rcpp::Named("wSex") = wSexes,
        Rcpp::Named("wYearBirth") = wYearBirths,
        Rcpp::Named("initiationAge") = initiationAge,
        Rcpp::Named("cessationAge") = cessationAge,
        Rcpp::Named("ageAtDeath") = ageAtDeath,
        Rcpp::Named("CPD") = cpdString
    );
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
       .method("LegacyRunWebVersion", &SHGInterface::LegacyRunWebVersion, "Runs a simulation from a configuration file to produce results for a website (legacy)");
   }

