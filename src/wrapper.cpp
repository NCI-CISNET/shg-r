// CISNET (www.cisnet.cancer.gov)
// Lung Cancer Base Case Group
// Smoking History Simulation Application
// Application to Simulate Initiation and Cessation Ages of individuals based on sex, race and year of birth.
// File: main.cpp
// Author: Martin Krapcho & Ben Racine
// E-Mail: KrapchoM@imsweb.com & ben.racine@cornerstonenw.com
// NCI Contact: Rocky Feuer
// Please view the HelpFile.txt file included with this source code for details pertaining to this version

// TODO: Update attribution and dates above

#pragma hdrstop
#pragma argsused

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

std::string VERSIONJSON;
std::string VERSION_NUM;
std::string gInputFileName;
bool gWithHoldTags = false;

const short wMIN_IMMEDIATE_CESSATION_YEAR = 1910;  // Minimum Year Value that can be used as the Immediatte Cessation Year
short wSIM_CUTOFF_YEAR = 2200;                     // Cut-off year for the application

const char sSEX_LABELS[2][7]  = {"Male", "Female"};
const char sRACE_LABELS[2][10] = {"All Races", "White"};

// Declaring Function prototypes
char* AssignFilename(const char* sDirectory, const char * sFilename);
short CountVectorValues(char* sDataString);
bool CreateDataFile(const char *sNumToSimulate, const char* sOutFileName, char*);
void Help(const char* sAppName, FILE* pHelpStream);
bool IsPosLongInt(const char *sValue);
bool IsPosShortInt(const char *sValue);
bool IsValidNumReps(const char* sNumReps);
bool IsValidSeed(const char* sSeedValue);
void LoadValue(char* sDest, char* sSource, int iValueNum);
void ModifyCutoffYear(char*);
bool RunFromParameters(const char*, char*, char*, char*, char*, char*, char*, char*, char*, char*);
void RunInfiniteLoop();
void RunInterface();
int RunWebVersion(const char *sInputFileName);
char* Str_toupper(char *s);
char* Str_tolower(char *s);
void Usage();
short min(short, short);
bool ValidateParameters(char*, char*, char*, char*, char*, char*, char*, char*, char*);
bool ValidateParameters(char*, char*, char*, char*, char*, char*, char*, char*, char*, char*);
void WriteInputTag(FILE* , char*, char*, const char*, const char*);
void WriteRunInfoTag(FILE* , std::string, const char*, const char*, const char*,
                     const char*, const char*, const char*, const char*, const char*,
                     const char*, const char*, const char*, const char*);
void PropagateVersionInformation();


#define STRICT_R_HEADERS
#include <Rcpp.h>
// [[Rcpp::depends(Rcpp)]]
// [[Rcpp::depends(rstream)]]

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
class SHGInterface
{
public:
   // Eventually we should probably allow for a constructor that takes seeds
   SHGInterface()
   {
      initialize();
   }

   void initialize()
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
   // A simple toggle between using rstream RNG versus C++ MT
   void setRNGtype(std::string RNGtype)
   {
      if (RNGtype == "rstream")
      {
         pSimulator->use_rstream = TRUE;
      }
      else
      {
         pSimulator->use_rstream = FALSE;
      }
   }

   void setRNGs(SEXP rng1, SEXP rng2, SEXP rng3, SEXP rng4)
   {

      // Ensure that the inputs are correct S4 rstream.mrg32k3a objects
      if (!Rf_inherits(rng1, "rstream.mrg32k3a") ||
          !Rf_inherits(rng2, "rstream.mrg32k3a") ||
          !Rf_inherits(rng3, "rstream.mrg32k3a") ||
          !Rf_inherits(rng4, "rstream.mrg32k3a"))
      {
         stop("All RNGs must be of class rstream.mrg32k3a.");
      }

      // Could also clone and advance substreams here instead of externally
      // Function next_substream = rstream_env["rstream.nextsubstream"]; //also resetsubstream
      pSimulator->gpInitiationPRNG_R = rng1;
      pSimulator->gpCessationPRNG_R = rng2;
      pSimulator->gpLifeTablePRNG_R = rng3;
      pSimulator->gpIndivRndsPRNG_R = rng4;
   }

   DataFrame runSim(int repeat)
   {

      short wRace = 0;
      short wSex = 0;
      short wYearBirth = 1950;
      FILE *pInputFile = 0,
           *pOutStream = 0,
           *pErrorStream = 0;
      const char *sOutputFile = "./out/test_output_from_module.txt";

      pOutStream = fopen(sOutputFile, "w");
      if (pOutStream == NULL)
      {
         fprintf(pErrorStream, "\n<ERROR>\nSupplied Output file: %s, could not be opened for writing.\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n", sOutputFile);
      }

      std::vector<int> initiationAge;
      std::vector<int> cessationAge;
      std::vector<int> ageAtDeath;
      std::vector<int> race;
      std::vector<int> sex;
      std::vector<int> yearBirth;
      std::vector<string> cpdString;
      string cpd;
      short wYearsAsSmoker, i;

      for (int j = 0; j < repeat; j++)
      {
         pSimulator->RunSimulationIndividual(wRace, wSex, wYearBirth, pOutStream);

         // For now these 3 are typically all the same in a given run, but we could add more variation
         race.push_back(wRace);
         sex.push_back(wSex);
         yearBirth.push_back(wYearBirth);

         initiationAge.push_back(pSimulator->gwPersonsInitAge);
         cessationAge.push_back(pSimulator->gwPersonsCessAge);
         ageAtDeath.push_back(pSimulator->gwPersonsAgeAtDeath);

         // Print out the smoking intensity group for the person and the cigarettes smoked per day
         // Print the intensity group as +1 its value so range of values is from 1 to 5.
         cpd = "";
         if (pSimulator->gwPersonsInitAge != -999)
         {
            if (pSimulator->gwPersonsCessAge == -999)
               wYearsAsSmoker = wSIM_CUTOFF_YEAR - (pSimulator->gwPersonsYOB + pSimulator->gwPersonsInitAge) + 1;
            else
               wYearsAsSmoker = pSimulator->gwPersonsCessAge - pSimulator->gwPersonsInitAge + 1;
            for (i = 0; i < wYearsAsSmoker; i++)
            {
               if (i + pSimulator->gwPersonsInitAge < 100)
                  cpd += std::to_string(i + pSimulator->gwPersonsInitAge) + " (" + std::to_string(static_cast<int>(pSimulator->gdPersonsCPDbyAge[i])) + "), ";
            }
         }
         cpdString.push_back(cpd);
      }

      fclose(pOutStream);

      DataFrame df = DataFrame::create(
          _["race"] = race,
          _["sex"] = sex,
          _["yob"] = yearBirth,
          _["smoking_initiation_age"] = initiationAge,
          _["smoking_cessation_age"] = cessationAge,
          _["oc_age_at_death"] = ageAtDeath,
          _["smoking_cpd"] = cpdString);

      return df;
   }

   double GetNextInitRand()
   {
      return pSimulator->GetNextInitRand();
   }

   double GetNextCessRand_R()
   {
      return pSimulator->GetNextCessRand_R();
   }

   // Just testing the MT
   double GetNextCessRandMT()
   {
      return pSimulator->GetNextCessRandMT();
   }

   NumericVector GetNextCessRand_R_vector(int n)
   {
      return pSimulator->GetNextCessRand_R_vector(n);
   }

   void RunWebVersion(const char *sInputFileName)
   {
      RunWebVersion(sInputFileName);
   }

   const char *sInputFile;
   const char *sOutputFile;
   Smoking_Simulator *pSimulator = 0;
};

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
       .method("initialize", &SHGInterface::initialize, "Test")
       .method("setRNGs", &SHGInterface::setRNGs, "Test")
       .method("GetNextCessRand_R", &SHGInterface::GetNextCessRand_R, "Test")
       .method("GetNextInitRand", &SHGInterface::GetNextInitRand, "Test")
       .method("setRNGtype", &SHGInterface::setRNGtype, "Test")
       .method("GetNextCessRand_R_vector", &SHGInterface::GetNextCessRand_R_vector, "Test")
       .method("GetNextCessRandMT", &SHGInterface::GetNextCessRandMT, "Test");
   }
//' RunWebVersion
//'
//' Executes a simluation based on parameters from a single input file
//'
//' @return bool
//' @examples
//' RunWebVersion()
// [[Rcpp::export]]
bool RunWebVersion() {
   // Just stubbing this in here for reference in case we want to avoid the RCPP_MODULE macro
   // depends on having an input file which we probably don't want
   const char *sInputFileName = "./inst/inputs/test_input.txt";
   // RunFromParameters(sInputFileName, ...);
   RunWebVersion(sInputFileName);
   return true;
}

// Returns a string containing the directory and filename concatenated together
char* AssignFilename(const char* sDirectory, const char * sFilename) {
   int iCurrIndex;
   char* sFullFilePath;
   sFullFilePath = new char[strlen(sDirectory) + strlen(sFilename) + 2];
   iCurrIndex = 0;
   for (unsigned int i=0; i <(strlen(sDirectory)); i++) {
      sFullFilePath[iCurrIndex] = sDirectory[i];
      iCurrIndex++;
   }
   #ifdef WIN32  // windows code goes here
      if (sFullFilePath[iCurrIndex-1] != '\\') {
         sFullFilePath[iCurrIndex] = '\\';
         iCurrIndex++;
      }
   #else // unix code goes here
      if (sFullFilePath[iCurrIndex-1] != '/') {
         sFullFilePath[iCurrIndex] = '/';
         iCurrIndex++;
      }
   #endif
   for (unsigned int i=0; i <(strlen(sFilename)); i++) {
      sFullFilePath[iCurrIndex] = sFilename[i];
      iCurrIndex++;
   }
   sFullFilePath[iCurrIndex] = '\0';
   return sFullFilePath;
}
// Runs the application using a single data file containing all necessary information.
// The output from this run is written in XML-style tags
int RunWebVersion(const char * sInputFileName)
{
   bool bRunApp = true,
        bHaveVectorValues = false,
        bUseNumReps = false;

   char sInputLine[1000],
        *sErrorFile      = 0,
        *sInputBuffer    = 0,
        *sFILE_InitProb  = 0, // Datafile - Initiation Probabilities
        *sFILE_CessProb  = 0, // Datafile - Cessation Probabilities
        *sFILE_OCDProb   = 0, // Datafile - Life Table (Probability of Dying from Cause other than Lung Cancer)
        *sFILE_Quintiles = 0, // Datafile - Smoking Intensity Quintile Placement Probabilites
        *sFILE_CPDData   = 0, // Datafile - Smoking Intensity - Cigarettes per Day by Quintile
        *sSEED_Init      = 0, // Seed - For PRNG that generates Initiation Probabilities
        *sSEED_Cess      = 0, // Seed - For PRNG that generates Cessation Probabilities
        *sSEED_OCD       = 0, // Seed - For PRNG that generates Death from Other OCD probabilities
        *sSEED_Misc      = 0, // Seed - For PRNG that generates miscellaneous random numbers that are needed for 1 time use for a person
        *sOutputFile     = 0,
        *sImmediateCess  = 0, // Immediate Cessation Year, 0 = do not do immediate cessation
        *sPARAM_Sex      = 0, // Run Parameter - Sex
        *sPARAM_Race     = 0, // Run Parameter - Race
        *sPARAM_YOB      = 0, // Run Parameter - Year of Birth
        *sPARAM_NumReps  = 0, // Run Parameter - Number of time to repeat current set of parameters
        sVecValues[4][20];

   FILE *pInputFile   = 0,
        *pOutStream   = 0,
        *pErrorStream = 0;

   int   iCurrIndex,
         iIndexLength,
         iReturnValue,
         iStringLength,
         i;

   long  lNumReps = 0;
   long  lSeed_Init,
         lSeed_Cess,
         lSeed_OCD,
         lSeed_Misc,
         j;

   short wValuesPerParam[4],
         wMaxNumPerParam,
         wCessationYear;

   Smoking_Simulator *pSimulator = 0;

   gInputFileName = sInputFileName;

   pInputFile = fopen(sInputFileName,"r");

   if (pInputFile == NULL) {
      Rcpp::Rcout << "The specified input file does not exist or could not be opened.\n" << sInputFileName;
      bRunApp = false;
   }

   if (bRunApp) {

      while (fgets(sInputLine, 1000, pInputFile) != NULL) {

         sInputBuffer = new char[strlen(sInputLine) + 1];
         strcpy(sInputBuffer, sInputLine);
         iIndexLength = 0;
         iStringLength = strlen(sInputBuffer);

         if (strstr(sInputBuffer, "\r") != NULL) {
            iStringLength--;
         }

         if (strstr(sInputBuffer,"\n")!=NULL) {
            iStringLength--;
         }

         if (strncmp(Str_toupper(sInputBuffer), "SEED_INIT=", strlen("SEED_INIT=")) == 0) {
            iIndexLength = strlen("SEED_INIT=");
            sSEED_Init = new char[(iStringLength - iIndexLength) + 1];
            iCurrIndex = 0;
            for ( i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputBuffer[i + iIndexLength] != ' ') {
                  sSEED_Init[iCurrIndex] = sInputBuffer[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sSEED_Init[iCurrIndex] = '\0';
         }

         if (strncmp(Str_toupper(sInputBuffer), "SEED_CESS=", strlen("SEED_CESS=")) == 0) {
            iIndexLength = strlen("SEED_CESS=");
            sSEED_Cess = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputBuffer[i + iIndexLength]!= ' ') {
                  sSEED_Cess[iCurrIndex] = sInputBuffer[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sSEED_Cess[iCurrIndex] = '\0';
         }

         if (strncmp(Str_toupper(sInputBuffer), "SEED_OCD=", strlen("SEED_OCD=")) == 0) {
            iIndexLength = strlen("SEED_OCD=");
            sSEED_OCD = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputBuffer[i + iIndexLength]!= ' ') {
                  sSEED_OCD[iCurrIndex] = sInputBuffer[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sSEED_OCD[iCurrIndex]='\0';
         }

         if (strncmp(Str_toupper(sInputBuffer), "SEED_MISC=", strlen("SEED_MISC=")) == 0) {
            iIndexLength = strlen("SEED_MISC=");
            sSEED_Misc = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputBuffer[i + iIndexLength]!= ' ') {
                  sSEED_Misc[iCurrIndex] = sInputBuffer[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sSEED_Misc[iCurrIndex]='\0';
         }

         if (strstr(Str_toupper(sInputBuffer), "SEX=") != NULL) {
            iIndexLength = strlen("SEX=");
            sPARAM_Sex = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for ( i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputBuffer[i + iIndexLength] != ' ') {
                  sPARAM_Sex[iCurrIndex] = sInputBuffer[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sPARAM_Sex[iCurrIndex]='\0';
            if (strstr(sPARAM_Sex, ", ") != NULL)
               bHaveVectorValues = true;
         }

         if (strstr(Str_toupper(sInputBuffer), "RACE=") != NULL) {
            iIndexLength = strlen("RACE=");
            sPARAM_Race = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputBuffer[i + iIndexLength]!= ' ') {
                  sPARAM_Race[iCurrIndex] = sInputBuffer[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sPARAM_Race[iCurrIndex]='\0';
            if (strstr(sPARAM_Race, ", ") != NULL)
               bHaveVectorValues = true;
         }

         if (strstr(Str_toupper(sInputBuffer), "YOB=") != NULL) {
            iIndexLength = strlen("YOB=");
            sPARAM_YOB = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputBuffer[i + iIndexLength]!= ' ') {
                  sPARAM_YOB[iCurrIndex] = sInputBuffer[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sPARAM_YOB[iCurrIndex]='\0';
            if (strstr(sPARAM_YOB, ", ") != NULL)
               bHaveVectorValues = true;
         }

         if (strstr(Str_toupper(sInputBuffer), "REPEAT=") != NULL) {
            iIndexLength = strlen("REPEAT=");
            sPARAM_NumReps = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputBuffer[i + iIndexLength]!= ' ') {
                  sPARAM_NumReps[iCurrIndex] = sInputBuffer[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sPARAM_NumReps[iCurrIndex]='\0';
            if (strstr(sPARAM_NumReps, ", ") != NULL)
               bHaveVectorValues = true;
         }

         if (strstr(Str_toupper(sInputBuffer), "INIT_PROB=") != NULL) {
            iIndexLength = strlen("INIT_PROB=");
            sFILE_InitProb = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputLine[i + iIndexLength]!= ' ') {
                  sFILE_InitProb[iCurrIndex] = sInputLine[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sFILE_InitProb[iCurrIndex]='\0';
         }

         if (strstr(Str_toupper(sInputBuffer), "CESS_PROB=") != NULL) {
            iIndexLength = strlen("CESS_PROB=");
            sFILE_CessProb = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputLine[i + iIndexLength]!= ' ') {
                  sFILE_CessProb[iCurrIndex] = sInputLine[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sFILE_CessProb[iCurrIndex]='\0';
         }

         if (strstr(Str_toupper(sInputBuffer), "OCD_PROB=") != NULL) {
            iIndexLength = strlen("OCD_PROB=");
            sFILE_OCDProb = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputLine[i + iIndexLength]!= ' ') {
                  sFILE_OCDProb[iCurrIndex] = sInputLine[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sFILE_OCDProb[iCurrIndex]='\0';
         }

         if (strstr(Str_toupper(sInputBuffer), "CPD_QUINTILES=") != NULL) {
            iIndexLength = strlen("CPD_QUINTILES=");
            sFILE_Quintiles = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputLine[i + iIndexLength]!= ' ') {
                  sFILE_Quintiles[iCurrIndex] = sInputLine[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sFILE_Quintiles[iCurrIndex]='\0';
         }

         if (strstr(Str_toupper(sInputBuffer), "CPD_DATA=") != NULL) {
            iIndexLength = strlen("CPD_DATA=");
            sFILE_CPDData = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputLine[i + iIndexLength]!= ' ') {
                  sFILE_CPDData[iCurrIndex] = sInputLine[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sFILE_CPDData[iCurrIndex]='\0';
         }

         if (strstr(Str_toupper(sInputBuffer), "OUTPUTFILE=") != NULL) {
            iIndexLength = strlen("OUTPUTFILE=");
            sOutputFile = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputLine[i + iIndexLength]!= ' ') {
                  sOutputFile[iCurrIndex] = sInputLine[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sOutputFile[iCurrIndex]='\0';
         }

         if (strstr(Str_toupper(sInputBuffer), "ERRORFILE=") != NULL) {
            iIndexLength = strlen("ERRORFILE=");
            sErrorFile = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputLine[i + iIndexLength]!= ' ') {
                  sErrorFile[iCurrIndex] = sInputLine[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sErrorFile[iCurrIndex]='\0';
         }

         if (strstr(Str_toupper(sInputBuffer), "IMMEDIATECESS=") != NULL) {
            iIndexLength = strlen("IMMEDIATECESS=");
            sImmediateCess = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputLine[i + iIndexLength]!= ' ') {
                  sImmediateCess[iCurrIndex] = sInputLine[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sErrorFile[iCurrIndex]='\0';
         }

         if (strstr(Str_toupper(sInputBuffer), "NOTAGS") != NULL) {
            gWithHoldTags = true;
         }

         delete [] sInputBuffer;
         sInputBuffer = 0;
         } // end While

      fclose(pInputFile);

      // Check for the error file string, open it if it exists, otherwise, open the default error file
      if (sErrorFile == NULL) {
         Rcpp::Rcout << "Name for Error log file was not found in input file: " << sInputFileName;
         bRunApp = false;
      } else {
         pErrorStream = fopen(sErrorFile,"w");
      }

      if (bRunApp && pErrorStream == NULL) {
	      Rcpp::Rcout << "Specified error file could not be opened for writing: " << sErrorFile;
         bRunApp = false;
      }
   }

   if (bRunApp) {
      // Make sure all necessary values were received
      // Check Seeds
      if (sSEED_Init == NULL) {
         fprintf(pErrorStream,"\n<ERROR>\nSeed for Initiation PRNG was not found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sInputFileName);
         bRunApp = false;
      } else if (!IsValidSeed(sSEED_Init)) {
         fprintf(pErrorStream,"\n<ERROR>\nInvalid Initiation PRNG Seed: %s found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sSEED_Init,sInputFileName);
         bRunApp = false;
      }
      if (sSEED_Cess == NULL) {
         fprintf(pErrorStream,"\n<ERROR>\nSeed for Cessation PRNG was not found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sInputFileName);
         bRunApp = false;
      } else if (!IsValidSeed(sSEED_Cess)) {
         fprintf(pErrorStream,"\n<ERROR>\nInvalid Cessation PRNG Seed: %s found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sSEED_Cess,sInputFileName);
         bRunApp = false;
      }
      if (sSEED_OCD == NULL) {
         fprintf(pErrorStream,"\n<ERROR>\nSeed for OCD PRNG was not found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sInputFileName);
         bRunApp = false;
      } else if (!IsValidSeed(sSEED_OCD)) {
         fprintf(pErrorStream,"\n<ERROR>\nInvalid OCD PRNG Seed: %s found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sSEED_OCD,sInputFileName);
         bRunApp = false;
      }
      if (sSEED_Misc == NULL) {
         fprintf(pErrorStream,"\n<ERROR>\nSeed for Miscellaneous PRNG was not found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sInputFileName);
         bRunApp = false;
      } else if (!IsValidSeed(sSEED_Misc)) {
         fprintf(pErrorStream,"\n<ERROR>\nInvalid Miscellaneous PRNG Seed: %s found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sSEED_Misc,sInputFileName);
         bRunApp = false;
      }

      // Check Files
      if (sFILE_InitProb == NULL) {
         fprintf(pErrorStream,"\n<ERROR>\nInitiation Probabilities file was not found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sInputFileName);
         bRunApp = false;
      }
      if (sFILE_CessProb == NULL) {
         fprintf(pErrorStream,"\n<ERROR>\nCessation Probabilities file was not found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sInputFileName);
         bRunApp = false;
      }
      if (sFILE_OCDProb == NULL) {
         fprintf(pErrorStream,"\n<ERROR>\nOCD Probabilities file was not found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sInputFileName);
         bRunApp = false;
      }
      if (sFILE_CPDData == NULL) {
         fprintf(pErrorStream,"\n<ERROR>\nCPD Data file was not found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sInputFileName);
         bRunApp = false;
      }
      if (sOutputFile == NULL) {
         fprintf(pErrorStream,"\n<ERROR>\nOutput file was not found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sInputFileName);
         bRunApp = false;
      }

      // Check parameters
      if (sPARAM_Sex == NULL) {
         fprintf(pErrorStream,"\n<ERROR>\nSex value(s) was not found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sInputFileName);
         bRunApp = false;
      }
      if (sPARAM_Race == NULL) {
         fprintf(pErrorStream,"\n<ERROR>\nRace value(s) was not found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sInputFileName);
         bRunApp = false;
      }
      if (sPARAM_YOB == NULL) {
         fprintf(pErrorStream,"\n<ERROR>\nYear of Birth value(s) was not found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sInputFileName);
         bRunApp = false;
      }

      // Check the optional sPARAM_NumReps value if we are not using a vector
      if (sPARAM_NumReps != NULL && !bHaveVectorValues && !IsValidNumReps(sPARAM_NumReps)) {
         fprintf(pErrorStream,"\n<ERROR>\nInvalid Number of Repetitions: %s,\n Value must be a positive integer with a max value of %d.\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sPARAM_NumReps,MAX_NUM_REPS);
         bRunApp = false;
      }
      else if (sPARAM_NumReps!=NULL && !bHaveVectorValues) {
         bUseNumReps = true;
         lNumReps    = atol(sPARAM_NumReps);
      } else if (sPARAM_NumReps!=NULL && bHaveVectorValues) {
         bUseNumReps = true;
      } else {
         bUseNumReps = false;
      }

   }  // end if (bRunApp)


   if (bRunApp) {
      // Still can run, try to open the output file
      pOutStream   = fopen(sOutputFile,"w");
      if (pOutStream == NULL) {
         fprintf(pErrorStream,"\n<ERROR>\nSupplied Output file: %s, could not be opened for writing.\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",sOutputFile);
         bRunApp = false;
      }
   }

   // Parse the Vector values if applicable
   if (bRunApp && bHaveVectorValues) {
      wValuesPerParam[0] = CountVectorValues(sPARAM_Race);
      wValuesPerParam[1] = CountVectorValues(sPARAM_Sex);
      wValuesPerParam[2] = CountVectorValues(sPARAM_YOB);
      wValuesPerParam[3] = CountVectorValues(sPARAM_NumReps);
      wMaxNumPerParam = 1;
      for (i=0; i < 4; i++) {
      	if ((wValuesPerParam[i] > wMaxNumPerParam) &&
             (wMaxNumPerParam > 1) ||
             (wMaxNumPerParam > 1 &&
              (wValuesPerParam[i] != wMaxNumPerParam && wValuesPerParam[i] > 1))) {

            bRunApp = false;
            fprintf(pErrorStream, "\n<ERROR>");
            fprintf(pErrorStream, "\nInvalid use of vector values in the input file.");
            fprintf(pErrorStream, "\nIf vector values are used for more than 1 variable,");
            fprintf(pErrorStream, "\nthe same number of values must be supplied for each variable.");
            fprintf(pErrorStream, "\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n");
	      } else if (wValuesPerParam[i] > wMaxNumPerParam) {
	         wMaxNumPerParam = wValuesPerParam[i];
	      }
	   } // end for
   } // end if (bRunApp && bHaveVectorValues)

   if (bRunApp) {
      lSeed_Init = atol(sSEED_Init);
      if (lSeed_Init == -1)
         lSeed_Init = time(0);
      lSeed_Cess = atol(sSEED_Cess);
      if (lSeed_Cess == -1)
         lSeed_Cess = time(0);
      lSeed_OCD = atol(sSEED_OCD);
      if (lSeed_OCD == -1)
         lSeed_OCD = time(0);
      lSeed_Misc = atol(sSEED_Misc);
      if (lSeed_Misc == -1)
         lSeed_Misc = time(0);

      if (sImmediateCess == NULL) {
         wCessationYear = 0;
      } else {
         wCessationYear = (short) atoi(sImmediateCess);
      }

      try {

         pSimulator = new Smoking_Simulator(sFILE_InitProb,  sFILE_CessProb,
                                            sFILE_OCDProb,   sFILE_Quintiles,
                                            sFILE_CPDData,   (unsigned long) atoi(sSEED_Init),
                                            (unsigned long) atoi(sSEED_Cess), (unsigned long) atoi(sSEED_OCD),
                                            (unsigned long) atoi(sSEED_Misc), 1,
                                            wCessationYear);

         // Measure & build input data string
         if (!gWithHoldTags) {
            WriteRunInfoTag(pOutStream,VERSION_NUM, sSEED_Init,sSEED_Cess, sSEED_OCD,
                         sSEED_Misc, sImmediateCess, sFILE_InitProb, sFILE_CessProb, sFILE_OCDProb,
                         sFILE_Quintiles, sFILE_CPDData, sOutputFile, sErrorFile);
         }

      } catch (SimException ex) {
	      fprintf(pErrorStream, "\n<ERROR>\n%s\n</ERROR>\n", ex.GetError());
         fprintf(pErrorStream, "<CALLPATH>\n%s\n</CALLPATH>", ex.GetCallPath());
         bRunApp = (ex.GetType() == SimException::NON_FATAL);
      }
   }

   if (bRunApp) {
      if (bHaveVectorValues) {
         if (wValuesPerParam[0] <= 1)
            strcpy(sVecValues[0], sPARAM_Race);
         if (wValuesPerParam[1] <= 1)
            strcpy(sVecValues[1], sPARAM_Sex);
         if (wValuesPerParam[2] <= 1)
            strcpy(sVecValues[2], sPARAM_YOB);
         if (bUseNumReps && wValuesPerParam[3] <= 1)
            strcpy(sVecValues[3], sPARAM_NumReps);
         else if (!bUseNumReps)
            strcpy(sVecValues[3], "\0");

         for (i = 0; i < wMaxNumPerParam && bRunApp; i++) {
            if (wValuesPerParam[0] > 1)
               LoadValue(sVecValues[0], sPARAM_Race, i);
            if (wValuesPerParam[1] > 1)
               LoadValue(sVecValues[1], sPARAM_Sex, i);
            if (wValuesPerParam[2] > 1)
               LoadValue(sVecValues[2], sPARAM_YOB, i);
            if (bUseNumReps && wValuesPerParam[3] > 1)
	            LoadValue(sVecValues[3], sPARAM_NumReps, i);

            if (!gWithHoldTags) {
               fprintf(pOutStream, "<SIMULATION>\n");
            }
            WriteInputTag(pOutStream, sVecValues[0], sVecValues[1], sVecValues[2], sVecValues[3]);
            if (!gWithHoldTags) {
               fprintf(pOutStream, "<RUN>\n");
            }

            if (bUseNumReps && !IsValidNumReps(sVecValues[3])) {
	            fprintf(pErrorStream, "\n<ERROR>\nInvalid Number of Repetitions: %s, \n Value must be a positive integer with a max value of %d.\n</ERROR>", sVecValues[3], MAX_NUM_REPS);
               fprintf(pErrorStream, "\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>");
               fprintf(pOutStream, "<RESULT>\nERROR\n</RESULT>\n</RUN>\n</SIMULATION>\n");
            } else if (bUseNumReps) {
               lNumReps = atol(sVecValues[3]);
               for (j=0; j<lNumReps && bRunApp;j++) {
                  try {
                     pSimulator->RunSimulationIndividual(atoi(sVecValues[0]), atoi(sVecValues[1]),
                                               atoi(sVecValues[2]), pOutStream);
                  } catch (SimException ex) {
            	      fprintf(pErrorStream,"\n<ERROR>\n%s\n</ERROR>\n",ex.GetError());
                     fprintf(pErrorStream,"<CALLPATH>\n%s\n</CALLPATH>",ex.GetCallPath());
                     bRunApp = (ex.GetType() == SimException::NON_FATAL);
                     fprintf(pOutStream,"<RESULT>\nERROR\n</RESULT>\n");
                  }
               }
               fprintf(pOutStream,"</RUN>\n</SIMULATION>\n");
            } else {
               try {
                  pSimulator->RunSimulationIndividual(atoi(sVecValues[0]), atoi(sVecValues[1]),
                                            atoi(sVecValues[2]), pOutStream);
               } catch (SimException ex) {
           	      fprintf(pErrorStream,"\n<ERROR>\n%s\n</ERROR>\n",ex.GetError());
                  fprintf(pErrorStream,"<CALLPATH>\n%s\n</CALLPATH>",ex.GetCallPath());
                  bRunApp = (ex.GetType() == SimException::NON_FATAL);
                  fprintf(pOutStream,"<RESULT>\nERROR\n</RESULT>\n");
               }
               if (!gWithHoldTags)
                  fprintf(pOutStream,"</RUN>\n</SIMULATION>\n");
            }
	      }
      } else if (bUseNumReps) {
         if (!gWithHoldTags)
            fprintf(pOutStream,"<SIMULATION>\n");
         WriteInputTag(pOutStream,sPARAM_Race,sPARAM_Sex,sPARAM_YOB,sPARAM_NumReps);
         if (!gWithHoldTags)
            fprintf(pOutStream,"<RUN>\n");
         for (j=0; j<lNumReps && bRunApp; j++) {
            try {
               pSimulator->RunSimulationIndividual(atoi(sPARAM_Race),atoi(sPARAM_Sex),atoi(sPARAM_YOB),pOutStream);
            } catch(SimException ex) {
        	      fprintf(pErrorStream,"\n<ERROR>\n%s\n</ERROR>\n",ex.GetError());
               fprintf(pErrorStream,"<CALLPATH>\n%s\n</CALLPATH>",ex.GetCallPath());
               bRunApp = (ex.GetType() == SimException::NON_FATAL);
               fprintf(pOutStream,"<RESULT>\nERROR\n</RESULT>\n");
            }
         }
         if (!gWithHoldTags)
            fprintf(pOutStream,"</RUN>\n</SIMULATION>\n");
      } else {
         try {
            fprintf(pOutStream,"<SIMULATION>\n");
            WriteInputTag(pOutStream,sPARAM_Race,sPARAM_Sex,sPARAM_YOB,sPARAM_NumReps);
            fprintf(pOutStream,"<RUN>\n");
            pSimulator->RunSimulationIndividual(atoi(sPARAM_Race),atoi(sPARAM_Sex),atoi(sPARAM_YOB),pOutStream);
            if (!gWithHoldTags)
               fprintf(pOutStream,"</RUN>\n</SIMULATION>\n");
         } catch (SimException ex) {
            fprintf(pErrorStream,"\n<ERROR>\n%s\n</ERROR>\n",ex.GetError());
            fprintf(pErrorStream,"<CALLPATH>\n%s\n</CALLPATH>",ex.GetCallPath());
            bRunApp = (ex.GetType() == SimException::NON_FATAL);
            fprintf(pOutStream,"<RESULT>\nERROR\n</RESULT>\n");
            fprintf(pOutStream,"</RUN>\n</SIMULATION>\n");
         }
      }
   }

   if (pOutStream != NULL)
      fclose(pOutStream);

   if (pErrorStream != NULL)
      fclose(pErrorStream);

   if (bRunApp)
      iReturnValue = 1;
   else
      iReturnValue = 0;

   delete [] sErrorFile;
   delete [] sInputBuffer;
   delete [] sFILE_InitProb;
   delete [] sFILE_CessProb;
   delete [] sFILE_OCDProb;
   delete [] sFILE_Quintiles;
   delete [] sFILE_CPDData;
   delete [] sSEED_Init;
   delete [] sSEED_Cess;
   delete [] sSEED_OCD;
   delete [] sSEED_Misc;
   delete [] sOutputFile;
   delete [] sImmediateCess;
   delete [] sPARAM_Sex;
   delete [] sPARAM_Race;
   delete [] sPARAM_YOB;
   delete [] sPARAM_NumReps;
   delete    pSimulator;

   return iReturnValue;
}

// Verify that a string value is a valid positive long integer
bool IsPosLongInt(const char* sValue) {
   char sUpperValue[100];  // Max long value is shorter than 100 digits
   bool bReturnValue;
   long lUpperValue = MAX(long);

   Rcpp::stop(sUpperValue, "%ld", lUpperValue);

   bReturnValue = ((strspn( sValue, "0123456789" ) == strlen(sValue)) &&
                   ((strlen(sValue) < strlen(sUpperValue)) ||
                    ((strlen(sValue) == strlen(sUpperValue)) &&
                     (strcmp(sValue,sUpperValue)<=0))));

   return bReturnValue;
}

// Verify that a string value is a valid positive short integer
bool IsPosShortInt(const char* sValue) {
   char 	sUpperValue[100]; // Max short int value is shorter than 100 digits
   bool 	bReturnValue;
   short wUpperValue      = MAX(short);
   Rcpp::stop(sUpperValue,"%d", wUpperValue);
   bReturnValue = ((strspn( sValue, "0123456789" ) == strlen(sValue)) &&
                   ((strlen(sValue) < strlen(sUpperValue)) ||
                    ((strlen(sValue) == strlen(sUpperValue)) &&
                     (strcmp(sValue,sUpperValue) <= 0))));
   return bReturnValue;
}

// Verify the repeat= value is a valid input
bool IsValidNumReps(const char* sNumReps) {
   long wNumReps;
   bool bReturnValue= false;
   if (IsPosLongInt(sNumReps)) {
      wNumReps = atoi(sNumReps);
      if (wNumReps <= MAX_NUM_REPS)
         bReturnValue = true;
   }
   return bReturnValue;
}

// Verify that the seed= value is a valid input
bool IsValidSeed(const char* sSeedValue) {
   bool bReturnValue;
   bReturnValue = (strcmp(sSeedValue,"-1") == 0) || (IsPosLongInt(sSeedValue));
   return bReturnValue;
}

// Testing function - Runs an infinite loop
// Provided so that the calling function can tests its actions when this app does not respond after a set time
void RunInfiniteLoop() {
   bool bCanStop = false;
   while (!bCanStop);
}
// Returns the number of data values contained in sDataString
short CountVectorValues(char* sDataString) {
   short wReturnValue = 0;
   char  *pTokenPtr = 0,
         *sBuffer   = 0;

   if (sDataString != NULL) {  // Designed so that the ERStatus string will count as 1 if missing
      sBuffer = new char[strlen(sDataString)+1];
      strcpy(sBuffer, sDataString);
      pTokenPtr = strtok(sBuffer, VECTOR_DELIMITER);
      while (pTokenPtr != NULL) {
         wReturnValue++;
         pTokenPtr = strtok(NULL, VECTOR_DELIMITER);
      }
   }
   delete [] sBuffer;
   return wReturnValue;
}
//---------------------------------------------------------------------------
char* Str_toupper(char *s) {
	char* p = s;
	while (*s) {
		*s = toupper(*s);
		s++;
	}
	return p;
}

char* Str_tolower(char *s) {
	char* p = s;
	while (*s) {
		*s = tolower(*s);
		s++;
	}
	return p;
}
//Writes out tagged information about the program to pOutStream
void WriteRunInfoTag(FILE* pOutStream, std::string sVersion, const char* sInitSeed,
                     const char* sCessSeed, const char* sOCDSeed, const char* sMiscSeed,
                     const char* sImmediateCessYear, const char* sInitFile, const char* sCessFile,
                     const char* sOCDProbFile, const char* sQuintilesFile, const char* sCPDDataFile,
                     const char* sOutputFile, const char* sErrorFile) {

   if (pOutStream == NULL)
      throw SimException("WriteRunInfoTag()::ERROR","Output stream is not initialized.\n");

   fprintf(pOutStream,"<RUNINFO>\n");
   fprintf(pOutStream,"<VERSIONINFO>\n%s\n</VERSIONINFO>\n", VERSIONJSON.c_str());
   fprintf(pOutStream,"<SEEDS>\n<INIT_PRNG_SEED>\n%s\n</INIT_PRNG_SEED>\n", sInitSeed);
   fprintf(pOutStream,"<CESS_PRNG_SEED>\n%s\n</CESS_PRNG_SEED>\n", sCessSeed);
   fprintf(pOutStream,"<OCD_PRNG_SEED>\n%s\n</OCD_PRNG_SEED>\n", sOCDSeed);
   fprintf(pOutStream,"<MISC_PRNG_SEED>\n%s\n</MISC_PRNG_SEED>\n</SEEDS>\n", sMiscSeed);
   fprintf(pOutStream,"<DATAFILES>\n");
   fprintf(pOutStream,"<INPUT_FILE>\n%s\n</INPUT_FILE>\n", gInputFileName.c_str());
   fprintf(pOutStream,"<INITIATION>\n%s\n</INITIATION>\n", sInitFile);
   fprintf(pOutStream,"<CESSATION>\n%s\n</CESSATION>\n", sCessFile);
   fprintf(pOutStream,"<OCD>\n%s\n<OCD>\n", sOCDProbFile);
   fprintf(pOutStream,"<CIG_PER_DAY>\n%s\n</CIG_PER_DAY>\n</DATAFILES>\n", sCPDDataFile);
   fprintf(pOutStream,"<OUTFILES>\n<OUTPUT>\n%s\n</OUTPUT>\n", sOutputFile);
   fprintf(pOutStream,"<ERRORS>\n%s\n</ERRORS>\n</OUTFILES>\n", sErrorFile);
   fprintf(pOutStream,"<OPTIONS>\n<CESSATION_YR>\n%s\n</CESSATION_YR>\n</OPTIONS>\n</RUNINFO>\n", sImmediateCessYear);
}

//Writes out tagged information about the current run to pOutStream
void WriteInputTag(FILE* pOutStream, char* sRace, char* sSex, const char* sYearOfBirth, const char* sNumReps) {

   int iSex, iRace;

   try {
      if (pOutStream == NULL) {
         throw SimException("ERROR","Output stream is not initialized.\n");
      }

      iSex = atoi(sSex);
      iRace = atoi(sRace);

      if (!gWithHoldTags) {
         fprintf(pOutStream, "<INPUTS>\n");

         if (iRace >= 0 && iRace < Smoking_Simulator::NUM_RACES) {
            fprintf(pOutStream, "<RACE>\n%s\n</RACE>\n", sRACE_LABELS[iRace]);
         } else {
            fprintf(pOutStream, "<RACE>\n%d\n</RACE>\n", iRace);
         }

         if (iSex >= 0 && iSex < Smoking_Simulator::NUM_SEXES) {
            fprintf(pOutStream,"<SEX>\n%s\n</SEX>\n", sSEX_LABELS[iSex]);
         } else {
            fprintf(pOutStream,"<SEX>\n%d\n</SEX>\n", iSex);
         }

         fprintf(pOutStream,"<YOB>\n%s\n</YOB>\n",sYearOfBirth);
         if (sNumReps != NULL && (strcmp(sNumReps,"\0") != 0)) {
            fprintf(pOutStream,"<REPEAT>\n%s\n</REPEAT>\n", sNumReps);
   	   }
         fprintf(pOutStream,"</INPUTS>\n");
      }
   } catch (SimException ex) {
      ex.AddCallPath("WriteInputTag(FILE*,char*...)");
      throw ex;
   }
}

void ModifyCutoffYear(char* newCutoff) {
   wSIM_CUTOFF_YEAR = min(atoi(newCutoff), wSIM_CUTOFF_YEAR);
}

short min(short first, short second){
   if (first < second) {
      return first;
   } else {
      return second;
   }
}

void LoadValue(char* sDest, char* sSource, int iValueNum) {
   char *pTokenPtr = 0,
        *sBuffer = 0;
   int i;

   sBuffer = new char[strlen(sSource) + 1];
   strcpy(sBuffer, sSource);
   pTokenPtr = strtok(sBuffer, ",");
   if (iValueNum == 0) {
      strcpy(sDest, pTokenPtr);
   } else {
      for (i = 1; i <= iValueNum; i++) {
	      pTokenPtr = strtok(NULL, ",");
	      if (i == iValueNum) {
            strcpy(sDest, pTokenPtr);
         }
	   }
   }
   delete [] sBuffer;
}
