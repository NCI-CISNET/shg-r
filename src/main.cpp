// CISNET (www.cisnet.cancer.gov)
// CISNET Lung Cancer Group
// Smoking History Generator
// Application to Simulate Initiation and Cessation Ages of individuals based on sex, race and year of birth.
// File: main.cpp
// Author: Martin Krapcho & Ben Racine
// E-Mail: KrapchoM@imsweb.com & ben.racine@cornerstonenw.com
// NCI Contact: Rocky Feuer
// Please view the HelpFile.txt file included with this source code for details pertaining to this version

#if !defined(__GNUC__) && !defined(__clang__)
#pragma hdrstop
#pragma argsused
#endif

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
#include <fstream>

#include "smoking_sim.h"
#include "sim_exception.h"
#include "rng_strategy.h"

#ifdef IS_RCPP
  #include <Rcpp.h>
#endif

using namespace std;

#define MAX(x) (std::numeric_limits<x>::max())
#define DEFAULT_DATA_DIR const_cast<char*>("data/2017-05-03/")
#define COUNTERFACTUAL_DATA_DIR const_cast<char*>("data/counterfactual_inputs_jan_2009/")

// Input file names
#define INITIATION_DATA_FILE "lbc_shg_initiation.txt"
#define CESSATION_DATA_FILE "lbc_shg_cessation.txt"
#define OTHER_COD_DATA_FILE "lbc_smokehist_oc_mortality.txt"
#define CPD_INTENSITY_PROBS "lbc_smokehist_cpdintensityprobs.txt"
#define CPD_DATA_FILE "lbc_shg_cpd.txt"

#define MT_INIT_SEED_DEFAULT "1898587603"
#define MT_CESS_SEED_DEFAULT "1468371936"
#define MT_OCD_SEED_DEFAULT "1551308340"
#define MT_MISC_SEED_DEFAULT "1590227640"

const unsigned long RNGSTREAM_SEED_DEFAULT[6] = {12345, 12345, 12345, 12345, 12345, 12345};

#define VECTOR_DELIMITER ","
#define MAX_NUM_REPS 10000000
#define ERROR_MESSAGE_SIZE 1000

const char* VERSION_NUM = "6.4.0";
string gInputFileName;
bool gWithHoldTags = false;

const short wMIN_IMMEDIATE_CESSATION_YEAR = 1910;  // Minimum Year Value that can be used as the Immediatte Cessation Year
short wSIM_CUTOFF_YEAR = 2200;                     // Cut-off year for the application

const char sSEX_LABELS[2][7]  = {"Male", "Female"};
const char sRACE_LABELS[2][10] = {"All Races", "White"};

// Declaring Function prototypes
char* AssignFilename(const char* sDirectory, const char * sFilename);
short CountVectorValues(char* sDataString);
bool CreateDataFile(const char *sNumToSimulate, const char* sOutFileName, char*);
bool IsPosLongInt(const char *sValue);
bool IsPosShortInt(const char *sValue);
bool IsValidNumReps(const char* sNumReps);
bool IsValidSeed(const char* sSeedValue);
void LoadValue(char* sDest, char* sSource, int iValueNum);
void ModifyCutoffYear(char*);
bool RunFromParameters(char*, char*, char*, char*, char*, char*, char*, char*, char*, char*);
void RunInterface();
int RunWebVersion(const char *sInputFileName);
char* Str_toupper(char *s);
char* Str_tolower(char *s);
void Usage();
short min(short, short);
bool ValidateParameters(char*, char*, char*, char*, char*, char*, char*, char*, char*);
bool ValidateParameters(char*, char*, char*, char*, char*, char*, char*, char*, char*, char*);
void WriteInputTag(FILE* , char*, char*, const char*, const char*);
void WriteRunInfoTag(FILE*, const char*, const char*, const char*, const char*,
                     const char*, const char*, const char*, const char*, const char*,
                     const char*, const char*, const char*, const char*, const char*, const char*);
string RngStreamToString(unsigned long arr[], int length);

// Removing the main() function for RCPP because it is unwanted
// But we include all the other methods and variables to avoid DRY violations in the Rcpp wrapper
#ifdef IS_RCPP
#else
int main(int argc, char* argv[]) {
	char sErrorMessage[1000];
	int iReturnValue;
   FILE* pHelpFile = 0;
   switch (argc) {

      // No input parameters, run the user-interface version
      case 1:
   		RunInterface();
	   	iReturnValue = 0; break;

      // 1 input parameter, could be a call Run the program for the web-based version of application
      case 2:
         iReturnValue = RunWebVersion(argv[1]);
         break;

      // 3 input parameters, Create a data file - FOR TESTING ONLY - NOT TO BE USED IN SIMULATIONS
      case 4:
         if ( (strcmp(argv[1], "CREATE_DATA_FILE") == 0) && CreateDataFile(argv[2], argv[3], sErrorMessage) ) {
      		iReturnValue = 0;
         } else {   // Must have hit an error
      		PrintError("%s\n", sErrorMessage);
      		iReturnValue = 1;
            getc(stdin);
        	} break;

      case 9:
         // Use Input parameters (no data directory assigned)
         if (ValidateParameters(argv[1], argv[2],argv[3],argv[4],argv[5],argv[6],argv[7],argv[8],sErrorMessage) &&
            RunFromParameters(DEFAULT_DATA_DIR,argv[1],argv[2],argv[3],argv[4],argv[5],argv[6],argv[7],argv[8],sErrorMessage)) {
            iReturnValue = 0;
         } else {  // We hit an error in Validating or Running the parameters, print error and exit
            PrintError("%s\n", sErrorMessage);
            iReturnValue = 1;
         } break;

      case 10:
         // Use Input parameters
         if (ValidateParameters(argv[1],argv[2],argv[3],argv[4],argv[5],argv[6],argv[7],argv[8],argv[9],sErrorMessage) &&
              RunFromParameters(argv[1],argv[2],argv[3],argv[4],argv[5],argv[6],argv[7],argv[8],argv[9],sErrorMessage)) {
            iReturnValue = 0;
         } else {  // We hit an error in Validating or Running the parameters, print error and exit
            PrintError("%s\n", sErrorMessage);
            iReturnValue = 1;
         } 
         break;

      case 11:
         ModifyCutoffYear(argv[10]);
         // Use Input parameters (no data directory assigned)
         if (ValidateParameters(argv[1],argv[2],argv[3],argv[4],argv[5],argv[6],argv[7],argv[8],sErrorMessage) &&
             RunFromParameters(DEFAULT_DATA_DIR,argv[1],argv[2],argv[3],argv[4],argv[5],argv[6],argv[7],argv[8],sErrorMessage)) {
            iReturnValue = 0;
         } else { // We hit an error in Validating or Running the parameters, print error and exit
            PrintError("%s\n", sErrorMessage);
            iReturnValue = 1;
         } break;

      case 12:
         ModifyCutoffYear(argv[11]);
         // Use Input parameters
         if (ValidateParameters(argv[1],argv[2],argv[3],argv[4],argv[5],argv[6],argv[7],argv[8],argv[9],sErrorMessage) &&
              RunFromParameters(argv[1],argv[2],argv[3],argv[4],argv[5],argv[6],argv[7],argv[8],argv[9],sErrorMessage)) {
            iReturnValue = 0;
         } else { // We hit an error in Validating or Running the parameters, print error and exit
            PrintError("%s\n", sErrorMessage);
            iReturnValue = 1;
         } 
         break;
      default:
   		Usage();
         break;
      }
   return iReturnValue;
}
#endif

// Returns a string containing the directory and filename concatenated together
char* AssignFilename(const char* sDirectory, const char * sFilename) {
   int iCurrIndex, i;
   char* sFullFilePath;
   sFullFilePath = new char[strlen(sDirectory) + strlen(sFilename) + 2];
   iCurrIndex = 0;
   for (i=0; i <(strlen(sDirectory)); i++) {
      sFullFilePath[iCurrIndex] = sDirectory[i];
      iCurrIndex++;
   }
   #ifdef WIN32
      if (sFullFilePath[iCurrIndex-1] != '\\') {
         sFullFilePath[iCurrIndex] = '\\';
         iCurrIndex++;
      }
   #else // posix
      if (sFullFilePath[iCurrIndex-1] != '/') {
         sFullFilePath[iCurrIndex] = '/';
         iCurrIndex++;
      }
   #endif
   for (i=0; i <(strlen(sFilename)); i++) {
      sFullFilePath[iCurrIndex] = sFilename[i];
      iCurrIndex++;
   }
   sFullFilePath[iCurrIndex] = '\0';
   return sFullFilePath;
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

// Run the application using the seeds and input/output stream
bool RunFromParameters(char* sDataFileDir, char* sInitiationSeed,
                      char* sCessationSeed, char* sOtherCODSeed,
                      char* sIndivRndSeed, char* sInputFile,
                      char* sOutputFile, char* sOutputType,
                      char* sImmediateCess, char* sErrorMessage) {

	bool						bReturnValue = true;
   short                wOutputType,
                        wCessationYear;
	unsigned long 			ulInitiationSeed,
					  			ulCessationSeed,
                        ulOtherCODSeed,
                        ulIndivRndSeed;
   char                *sInitiationFile = 0,
                       *sCessationFile = 0,
                       *sOtherCODFile = 0,
                       *sCPDIntensityFile = 0,
                       *sCPDDataFile = 0;
	Smoking_Simulator	  *pSimulator  = 0;

	try {
      sInitiationFile = AssignFilename(sDataFileDir, INITIATION_DATA_FILE);
      sCessationFile = AssignFilename(sDataFileDir, CESSATION_DATA_FILE);
      sOtherCODFile = AssignFilename(sDataFileDir, OTHER_COD_DATA_FILE);
      sCPDIntensityFile = AssignFilename(sDataFileDir, CPD_INTENSITY_PROBS);
      sCPDDataFile = AssignFilename(sDataFileDir, CPD_DATA_FILE);
      ulInitiationSeed = (unsigned long) atol(sInitiationSeed);
      ulCessationSeed = (unsigned long) atol(sCessationSeed);
      ulOtherCODSeed = (unsigned long) atol(sOtherCODSeed);
      ulIndivRndSeed = (unsigned long) atol(sIndivRndSeed);
      wOutputType = (short) atoi(sOutputType);
      wCessationYear = (short) atoi(sImmediateCess);

  		pSimulator = new Smoking_Simulator(sInitiationFile, sCessationFile, sOtherCODFile, sCPDIntensityFile, sCPDDataFile, 
                                         ulInitiationSeed, ulCessationSeed, ulOtherCODSeed, ulIndivRndSeed,  
                                         wOutputType, wCessationYear);

      pSimulator->RunSimulation(sInputFile, sOutputFile, false);

   } catch (SimException ex) {
      // * is replaced with 1000-1=999 characters to allow for the null terminator
      snprintf(sErrorMessage, ERROR_MESSAGE_SIZE, "%.*s", ERROR_MESSAGE_SIZE-1, ex.GetError());
      bReturnValue = false;
   } catch(...) {
      snprintf(sErrorMessage, ERROR_MESSAGE_SIZE, "Unknown Error Occurred\n");
		bReturnValue = false;
   }

	delete pSimulator;
   delete [] sInitiationFile; delete [] sCessationFile; delete [] sOtherCODFile; delete [] sCPDIntensityFile; delete [] sCPDDataFile;
	return bReturnValue;
}

// Verify that a string value is a valid positive long integer
bool IsPosLongInt(const char* sValue) {
   char sUpperValue[100];  // Max long value is shorter than 100 digits
   bool bReturnValue;
   long lUpperValue = MAX(long);

   snprintf(sUpperValue, sizeof(sUpperValue), "%ld", lUpperValue);

   bReturnValue = ((strspn( sValue, "0123456789" ) == strlen(sValue)) &&
                   ((strlen(sValue) < strlen(sUpperValue)) ||
                    ((strlen(sValue) == strlen(sUpperValue)) &&
                     (strcmp(sValue,sUpperValue)<=0))));

   return bReturnValue;
}

void GetInput(char* sInputChar, short length) {
   if (fgets(sInputChar, length, stdin) == NULL) {
      PrintError("Error reading input\n");
      #ifndef IS_RCPP
         exit(1);
      #endif
   }
   else {
      // Remove new line character from input
      sInputChar[strcspn(sInputChar, "\n")] = '\0';
   }
}

// Verify that a string value is a valid positive short integer
bool IsPosShortInt(const char* sValue) {
   char 	sUpperValue[100]; // Max short int value is shorter than 100 digits
   bool 	bReturnValue;
   short wUpperValue      = MAX(short);
   snprintf(sUpperValue, sizeof(sUpperValue), "%hd", wUpperValue);
   bReturnValue = ((strspn( sValue, "0123456789") == strlen(sValue)) &&
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
      if (wNumReps <= MAX_NUM_REPS && wNumReps >= 1)
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

//---------------------------------------------------------------------------
void RunInterface() {
   Smoking_Simulator* 	pSimulator = 0;
   char           		sInputChar[101],
                  		sOutputFileName[105],
                  		sInputFileName[105],
                  		sExtensionCheck[5],
                       *sInitiationFile = 0,
                       *sCessationFile = 0,
                       *sOtherCODFile = 0,
                       *sCPDIntensityFile = 0,
                       *sCPDDataFile = 0;
   bool                 bValidInput,
                  		bKeepRepeating;
   unsigned long  		ulInitPRNGSeed,
                  		ulCessPRNGSeed,
                        ulOthCODSeed,
                        ulIndivRndSeed;
   short                wSourceData,
                        wTempValue,
                  		wInputSex,
                  		wInputRace,
                  		wInputYOB,
                  		wInputOutputType,
                        wOutputFormat,
                        wCessationYear,
                  		i;
   long           		lExtCheckPosition,
                  		lNumRepetitions;
   FILE*          		pOutputFile = 0;

   PrintMessage("Smoking History Simulator\n\n");

   bValidInput = false;
   wCessationYear = 0; // 0 = do not use immediate cessation.
   PrintMessage("\nSelect which estimates to use as the model inputs:\n");
   PrintMessage("1 - NHIS estimates.\n");
   PrintMessage("2 - Counterfactual estimates.\n");
   PrintMessage("3 - Immediate Cessation using NHIS estimates.\n");
   PrintMessage("(Please enter 1, 2 or 3):\n");
   while (!bValidInput) {
      GetInput(sInputChar, 4);
      if (IsPosShortInt(sInputChar) && ((atoi(sInputChar) >= 1) && (atoi(sInputChar) <= 3))) {
         wSourceData = atoi(sInputChar);
         bValidInput = true;
         if (wSourceData == 3) {
            PrintMessageFormatted("\nB Enter a year to use for immediate cessation.\nAll smokers will quit smoking on Jan 1st of this year.\n(Please enter a year in the range %04d-%04d):\n",
                    wMIN_IMMEDIATE_CESSATION_YEAR, wSIM_CUTOFF_YEAR);

            PrintMessageFormatted("\nAEnter a year to use for immediate cessation.\nAll smokers will quit smoking on Jan 1st of this year.\n(Please enter a year in the range %04d-%04d):\n",
                    wMIN_IMMEDIATE_CESSATION_YEAR, wSIM_CUTOFF_YEAR);
            bValidInput = false;
            while (!bValidInput) {
               GetInput(sInputChar, 10);
               if (IsPosShortInt(sInputChar) &&
                   ((atoi(sInputChar) >= wMIN_IMMEDIATE_CESSATION_YEAR) &&
                    (atoi(sInputChar) <= wSIM_CUTOFF_YEAR)))
                  {
                  wCessationYear = atoi(sInputChar);
                  bValidInput = true;
                  }
               else
                  PrintMessageFormatted("%s\" - Invalid Input.\nPlease enter a value between %d and %d:\n",sInputChar,wMIN_IMMEDIATE_CESSATION_YEAR,wSIM_CUTOFF_YEAR);
               }

            }
         }
      else
         PrintMessageFormatted("\n\"%s\" - Invalid Input.\nPlease enter either 1, 2 or 3:\n", sInputChar);
      }

   /* Load the filenames for the application */
   if (wSourceData == 2) {
      sInitiationFile = AssignFilename(COUNTERFACTUAL_DATA_DIR, INITIATION_DATA_FILE);
      sCessationFile = AssignFilename(COUNTERFACTUAL_DATA_DIR, CESSATION_DATA_FILE);
      sOtherCODFile = AssignFilename(COUNTERFACTUAL_DATA_DIR, OTHER_COD_DATA_FILE);
      sCPDIntensityFile = AssignFilename(COUNTERFACTUAL_DATA_DIR, CPD_INTENSITY_PROBS);
      sCPDDataFile = AssignFilename(COUNTERFACTUAL_DATA_DIR, CPD_DATA_FILE);
   } else {
      sInitiationFile = AssignFilename(DEFAULT_DATA_DIR, INITIATION_DATA_FILE);
      sCessationFile = AssignFilename(DEFAULT_DATA_DIR, CESSATION_DATA_FILE);
      sOtherCODFile = AssignFilename(DEFAULT_DATA_DIR, OTHER_COD_DATA_FILE);
      sCPDIntensityFile = AssignFilename(DEFAULT_DATA_DIR, CPD_INTENSITY_PROBS);
      sCPDDataFile = AssignFilename(DEFAULT_DATA_DIR, CPD_DATA_FILE);
   }

   bValidInput         = false;
   PrintMessageFormatted("\nRandom Number Generator Seeds:\n");
   PrintMessage("Please enter a seed for the PRNG that generates Initiation Probabilities.\n");
   PrintMessageFormatted("Seed should be in range 0 - %ld.\n:", MAX(long));
   while (!bValidInput)
      {
      GetInput(sInputChar, 10);
      if (IsPosLongInt(sInputChar)) {
         ulInitPRNGSeed = (unsigned long) atol(sInputChar);
         bValidInput = true;
         }
      else
         PrintMessageFormatted("\n\"%s\" - Invalid Input.\nPlease enter a value in range 0 - %ld.\n:",
                 sInputChar,MAX(long));
      }

   bValidInput = false;
   PrintMessage("Please enter a seed for the PRNG that generates Cessation Probabilities.\n");
   PrintMessageFormatted("Seed should be in range 0 - %ld.\n:", MAX(long));
   while (!bValidInput) {
      GetInput(sInputChar, 20);
      if (IsPosLongInt(sInputChar)) {
         ulCessPRNGSeed = (unsigned long) atol(sInputChar);
         bValidInput = true;
      }
      else
         PrintMessageFormatted("\n\"%s\" - Invalid Input.\nPlease enter a value in range 0 - %ld.\n:",
                 sInputChar,MAX(long));
      }

   bValidInput = false;
   PrintMessage("Please enter a seed for the PRNG that generates \nnon-lung cancer death probabilities.\n");
   PrintMessageFormatted("Seed should be in range 0 - %ld.\n:", MAX(long));
   while (!bValidInput) {
      GetInput(sInputChar, 20);
      if (IsPosLongInt(sInputChar))
         {
         ulOthCODSeed = (unsigned long) atol(sInputChar);
         bValidInput = true;
         }
      else
         PrintMessageFormatted("\n\"%s\" - Invalid Input.\nPlease enter a value in range 0 - %ld.\n:",
                 sInputChar,MAX(long));
      }

   bValidInput = false;
   PrintMessage("Please enter a seed for the PRNG that generates \nunique random numbers for the simulated individual.\n");
   PrintMessage("This PRNG is for defining characteristics such as \nwill the person be a light or heavy smoker.\n");
   PrintMessageFormatted("Seed should be in range 0 - %ld.\n:", MAX(long));
   while (!bValidInput) {
      GetInput(sInputChar, 20);
      if ( IsPosLongInt(sInputChar))
         {
         ulIndivRndSeed = (unsigned long) atol(sInputChar);
         bValidInput = true;
      }
      else {
         PrintMessageFormatted("\n\"%s\" - Invalid Input.\nPlease enter a value in range 0 - %ld.\n:",
                 sInputChar,MAX(long));
      }
   }
   bValidInput = false;

   PrintMessageFormatted("\nData Input and Output Options:\n");
   PrintMessage("1 - Read values from a file and write results to an output file.\n");
   PrintMessage("2 - Read values from a file and write results to the screen only.\n");
   PrintMessage("3 - Manually enter Sex, Race and Year of Birth Values \n");
   PrintMessage("    and write results to an output file.\n");
   PrintMessage("4 - Manually enter Sex, Race and Year of Birth Values\n");
   PrintMessage("    and write results to the screen only.\n");
   PrintMessage("(Please enter 1 to 4):\n");

   while (!bValidInput) {
      GetInput(sInputChar, 10);
      if (IsPosShortInt(sInputChar) && 
           ((atoi(sInputChar) >= 1) && (atoi(sInputChar) <= 4))) {
         wInputOutputType = atoi(sInputChar);
         bValidInput = true;
      } else {
         PrintMessageFormatted("\n\"%s\" - Invalid Input.\nPlease enter a value 1 through 4:\n", sInputChar);
      }
   }

   if (wInputOutputType == 1 || wInputOutputType == 2) {
      PrintMessageFormatted("\nSpecify input filename (100 char max). Leave empty to use default ./data/test_input_rows.txt filename:\n");
      GetInput(sInputChar, 100);
      if (strlen(sInputChar) == 0) {
         strcpy(sInputFileName, "./data/test_input_rows.txt"); // Use default filename
      } else {
         strcpy(sInputFileName, sInputChar);
      }
   }

   if (wInputOutputType == 1 || wInputOutputType == 3) {
      PrintMessage("Specify an output filename (100 char max). Leave empty to use default ./test.out filename:\n");

      GetInput(sInputChar, 100);
      if (strlen(sInputChar) == 0) {
         strcpy(sOutputFileName, "./test.out"); // Use default filename
      } else {
         strcpy(sOutputFileName, sInputChar);
      }

      if (strlen(sInputChar) > 4) {
         lExtCheckPosition = strlen(sInputChar) - 4;
         for (i=0; i <=3; i++)
            {
            sExtensionCheck[i] = toupper(sInputChar[lExtCheckPosition + i]);
            }
         sExtensionCheck[4] = '\0';
      }

      if (wInputOutputType == 3) {
         pOutputFile = fopen(sOutputFileName, "w");
      }
   }

   bValidInput = false;
   PrintMessage( "\nOutput Format Options:\n");
   PrintMessage( "1 - Write the output as a comma-delimited data string.\n");
   PrintMessage( "2 - Write the output as plain text.\n");
   PrintMessage( "3 - Write the output in a timeline-style format.\n");
   PrintMessage( "(Please enter 1 to 3):\n");

   while (!bValidInput) {
      GetInput(sInputChar, 4);
      if (IsPosShortInt(sInputChar) && 
           ((atoi(sInputChar) >= 1) && (atoi(sInputChar) <= 3))) {
         wOutputFormat = atoi(sInputChar);
         bValidInput = true;
      } else {
         PrintMessageFormatted("\n\"%s\" - Invalid Input.\nPlease enter a value 1 through 3:\n", sInputChar);
      }
   }

   bValidInput = false;

   try {
      pSimulator = new Smoking_Simulator( sInitiationFile,   sCessationFile,
                                          sOtherCODFile,     sCPDIntensityFile,
                                          sCPDDataFile,        ulInitPRNGSeed,
                                          ulCessPRNGSeed,       ulOthCODSeed,
                                          ulIndivRndSeed,       wOutputFormat,
                                          wCessationYear);

      if (wInputOutputType == 1) {
         PrintMessageFormatted("\n\n");
         pSimulator->RunSimulation(sInputFileName, sOutputFileName, true);
      } else if (wInputOutputType == 2) {
         PrintMessageFormatted("\n\n");
         pSimulator->RunSimulation(sInputFileName);
      } else {  // manually enter sex, race, Year of Birth

         bKeepRepeating = true;

         while (bKeepRepeating) {
            wInputRace = 0; // Only All Races is available in this iteration of program
            PrintMessageFormatted("\nEnter a sex value. \n(0 = Male, 1 = Female):\n");
            bValidInput = false;
            while (!bValidInput) {
               GetInput(sInputChar, 4);
               if (IsPosShortInt(sInputChar) &&  
                    ((atoi(sInputChar) == 0) || (atoi(sInputChar) == 1))) {
                  wInputSex = (atoi(sInputChar));
                  bValidInput = true;
               } else {
                  PrintMessageFormatted("\n\"%s\" - Invalid Input.\nPlease enter 0 or 1:\n", sInputChar);
               }
            }

            PrintMessageFormatted("\nEnter a year of birth between %d and %d:\n",pSimulator->GetMinYearOfBirth(),pSimulator->GetMaxYearOfBirth());
            bValidInput = false;
            while (!bValidInput) {
               GetInput(sInputChar, 8);
               if (IsPosShortInt(sInputChar) &&
                    ((atoi(sInputChar) >= pSimulator->GetMinYearOfBirth()) &&
                     (atoi(sInputChar) <= pSimulator->GetMaxYearOfBirth()))) {
                  wInputYOB   = atoi(sInputChar);
                  bValidInput = true;
               } else {
                  PrintMessageFormatted("\n\"%s\" - Invalid Input.\nPlease enter a value between %d and %d:\n",
                         sInputChar,pSimulator->GetMinYearOfBirth(),pSimulator->GetMaxYearOfBirth());
               }
            }

            PrintMessageFormatted("\nNumber of persons to simulate for supplied values:\n");
            bValidInput = false;
            while (!bValidInput) {
               GetInput(sInputChar, 10);
               if (IsPosLongInt(sInputChar) && 
                  (atol(sInputChar) >= 1)) {
                     lNumRepetitions = atol(sInputChar);
                     bValidInput = true;
               } else {
                  PrintMessageFormatted("\n\"%s\" is not a valid value.\nAllowable range is 1 to %ld \nPlease enter a new value:\n", sInputChar, MAX(long));
               }
            }

            PrintMessageFormatted("\n");
            for (long j = 1; j <= lNumRepetitions; j++) {
               pSimulator->RunSimulationSingle(wInputRace, wInputSex, wInputYOB, pOutputFile);
               #ifndef IS_RCPP
                  pSimulator->WriteToStream(stdout);
               #endif
            }

            PrintMessage( "\nSimulations complete for supplied input.\n1 - Perform more simulations\n2 - Quit\n:");
            bValidInput = false;
            while (!bValidInput) {
               GetInput(sInputChar, 4);
               if (IsPosShortInt(sInputChar)) {
                  wTempValue = atoi(sInputChar);
                  if ((wTempValue != 1) && (wTempValue != 2)) {
                     PrintMessageFormatted("\n\"%s\" - Invalid Input.\nPlease enter 1 or 2:\n", sInputChar);
                  } else {
                     if (wTempValue == 2) {
                        bKeepRepeating = false;
                     }
                     bValidInput = true;
                  }
               } else {
                  PrintMessageFormatted("\n\"%s\" - Invalid Input.\nPlease enter 1 or 2:\n", sInputChar);
               }
            }

         } // while(bKeepRepeating)
      }
      PrintMessage("Simulations complete\nPress \"Enter\" to close this window\n");
      getc(stdin);
      }
   catch (SimException ex) {
      PrintMessage("Internal error occurred\n");
      PrintMessageFormatted("Error : '%s'\n", ex.GetError());
      getc(stdin);
   } catch (...) {
      PrintMessage("Unknown Error Occurred\n");
      getc(stdin);
   }

   if (pOutputFile != 0) {
      fclose(pOutputFile);
   }

   delete pSimulator;
   delete [] sInitiationFile; delete [] sCessationFile; delete [] sOtherCODFile; delete [] sCPDIntensityFile; delete [] sCPDDataFile;
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
        sErrorMessage[1000],
        *sRNGStrategy    = 0,
        *sInputBuffer    = 0,
        *sRngStreamSeed = 0,
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

   long  lNumReps,
         lSeed_Init,
         lSeed_Cess,
         lSeed_OCD,
         lSeed_Misc,
         j;
   unsigned long rngStreamSeed[6];
   memcpy(rngStreamSeed, RNGSTREAM_SEED_DEFAULT, sizeof(RNGSTREAM_SEED_DEFAULT));
   char *sFinalRngStreamSeed = strdup("12345,12345,12345,12345,12345,12345");

   short wValuesPerParam[4],
         wMaxNumPerParam,
         wCessationYear;

   Smoking_Simulator *pSimulator = 0;

   gInputFileName = sInputFileName;
   pInputFile = fopen(sInputFileName, "r");
   
   if (pInputFile == NULL) {
      // Config input file
      snprintf(sErrorMessage, sizeof(sErrorMessage), "The specified input file '%s' could not be opened for reading.\n", sInputFileName);
      #ifdef IS_RCPP 
        Rcpp::stop(sErrorMessage); // Warning in R?
      #else
        PrintError(sErrorMessage);
      #endif
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
            if (strchr(sPARAM_Sex, ',') != NULL)
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
            if (strchr(sPARAM_Race, ',') != NULL)
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
            if (strchr(sPARAM_YOB, ',') != NULL)
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
            if (strchr(sPARAM_NumReps, ',') != NULL)
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
         
         if (strstr(Str_toupper(sInputBuffer), "RNGSTRATEGY=") != NULL) {
            iIndexLength = strlen("RNGSTRATEGY=");
            sRNGStrategy = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputLine[i + iIndexLength]!= ' ') {
                  sRNGStrategy[iCurrIndex] = sInputLine[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sRNGStrategy[iCurrIndex]='\0';
         }

         // Note: RngStream's default seed is an array of 6 integers {12345,12345,12345,12345,12345,12345}
         // It is fine to use the default seed, but also good to allow the user to specify their own seed
         // Only one seed is requred for RngStream because it uses substreams to generate multiple (IID) streams
         if (strstr(Str_toupper(sInputBuffer), "RNGSTREAM_SEED=") != NULL) {
            iIndexLength = strlen("RNGSTREAM_SEED=");
            char* sRngStreamSeed = new char[(iStringLength - iIndexLength) + 1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputLine[i + iIndexLength] != ' ') {
                     sRngStreamSeed[iCurrIndex] = sInputLine[i + iIndexLength];
                     iCurrIndex++;
               }
            }
            sRngStreamSeed[iCurrIndex] = '\0';

            // Parse the comma-delimited string into an array of 6 unsigned long integers
            char* token = strtok(sRngStreamSeed, ",");
            int index = 0;
            while (token != NULL && index < 6) {
               rngStreamSeed[index] = atoi(token);
               token = strtok(NULL, ",");
               index++;
            }

            // Ensure we have exactly 6 values
            if (index != 6) {
               PrintError("Error: RNGSTREAM_SEED must contain exactly 6 comma-delimited integers.\n");
               // TODO we could/should also check for valid values here (see RngStream documentation)
               // Handle error appropriately (e.g., set a flag, exit, etc.)
            }

            delete[] sRngStreamSeed;
         }
         //string temp = RngStreamToString(rngStreamSeed, 6);
         //sFinalRngStreamSeed = strdup(temp.c_str());
         
         if (strstr(Str_toupper(sInputBuffer), "CESSATION_YR=") != NULL) {
            iIndexLength = strlen("CESSATION_YR=");
            sImmediateCess = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputLine[i + iIndexLength]!= ' ') {
                  sImmediateCess[iCurrIndex] = sInputLine[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sImmediateCess[iCurrIndex]='\0';
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
         PrintMessageFormatted("Name for Error log file was not found in input file: %s", sInputFileName);
         bRunApp = false;
      } else {
         pErrorStream = fopen(sErrorFile,"w");
      }

      if (sRNGStrategy == NULL) {   
         //PrintMessage("Using default random number strategy of RngStream because none was specified in input file: '%s'\n", sInputFileName);
          sRNGStrategy = const_cast<char*>("RngStream"); // strdup(DEFAULT_RNG_STRATEGY) not allowed
      }
      
      else if (strcmp(sRNGStrategy, "MersenneTwister") == 0) {
         //PrintMessage("Using Mersenne Twister random number generator strategy.\n");
      }
      else if (strcmp(sRNGStrategy, "RngStream") == 0 ) {
         //PrintMessage("Using RngStream random number generator strategy.\n");
      }
      else if (sRNGStrategy == nullptr || strlen(sRNGStrategy) == 0) {
         sRNGStrategy = const_cast<char*>("RngStream");
         //PrintMessage("Using default (RngStream) random number generator strategy.\n");
      }
      else {
         PrintError("The specified RNG strategy is invalid: '%s'\n", sRNGStrategy);
         bRunApp = false;
      }

      if (bRunApp && pErrorStream == NULL) {
         // Due to Rcpp not allowing variadic functions, we use snprintf to do substitution ***
         PrintError(sErrorMessage);
         snprintf(sErrorMessage, 1000, "Specified error file: '%s' could not be opened for writing.\n", sErrorFile);
         #ifdef IS_RCPP 
           Rcpp::stop(sErrorMessage); // Warning in R?
         #endif
         bRunApp = false;
      }
   } 
   if (bRunApp) {
      // Make sure all necessary values were received
        if (strcmp(sRNGStrategy, "MersenneTwister") == 0) {
         // Check MT Seeds
         if (sSEED_Init == NULL) {
            sSEED_Init = strdup(MT_INIT_SEED_DEFAULT);
         } 
         if (!IsValidSeed(sSEED_Init)) {
            WriteToFile(pErrorStream,"\n<ERROR>\nInvalid Initiation Seed: '%s' found in input file: '%s'\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                  sSEED_Init,sInputFileName);
            bRunApp = false;
         }
         if (sSEED_Cess == NULL) {
            // TODO: Check this as potential source of error; should consider using string everywhere
            PrintMessage("Using default Cessation Seed\n");
            sSEED_Cess = strdup(MT_CESS_SEED_DEFAULT);
            // This also works, but requires delete[] later
            //sSEED_Cess = new char[strlen(MT_CESS_SEED_DEFAULT) + 1];
            //strcpy(sSEED_Cess, MT_CESS_SEED_DEFAULT);
         }

         if (!IsValidSeed(sSEED_Cess)) {
            WriteToFile(pErrorStream,"\n<ERROR>\nInvalid Cessation Seed: '%s' found in input file: '%s'\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                  sSEED_Cess,sInputFileName);
            bRunApp = false;
         }
         if (sSEED_OCD == NULL) {
            sSEED_OCD = strdup(MT_OCD_SEED_DEFAULT);

         }
         if (!IsValidSeed(sSEED_OCD)) {
            WriteToFile(pErrorStream,"\n<ERROR>\nInvalid OCD Seed: '%s' found in input file: '%s'\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                  sSEED_OCD,sInputFileName);
            bRunApp = false;
         }
         if (sSEED_Misc == NULL) {
            sSEED_Misc = strdup(MT_MISC_SEED_DEFAULT);
         }
         if (!IsValidSeed(sSEED_Misc)) {
            WriteToFile(pErrorStream,"\n<ERROR>\nInvalid Miscellaneous Seed: '%s' found in input file: '%s'\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                  sSEED_Misc,sInputFileName);
            bRunApp = false;
         }
      }

      // Check Files
      if (sFILE_InitProb == NULL) {
         WriteToFile(pErrorStream,"\n<ERROR>\nInitiation Probabilities file was not found in input file: '%s'\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sInputFileName);
         bRunApp = false;
      }
      if (sFILE_CessProb == NULL) {
         WriteToFile(pErrorStream,"\n<ERROR>\nCessation Probabilities file was not found in input file: '%s'\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sInputFileName);
         bRunApp = false;
      }
      if (sFILE_OCDProb == NULL) {
         WriteToFile(pErrorStream,"\n<ERROR>\nOCD Probabilities file was not found in input file: '%s'\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sInputFileName);
         bRunApp = false;
      }
      if (sFILE_CPDData == NULL) {
         WriteToFile(pErrorStream,"\n<ERROR>\nCPD Data file was not found in input file: '%s'\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sInputFileName);
         bRunApp = false;
      }
      if (sOutputFile == NULL) {
         WriteToFile(pErrorStream,"\n<ERROR>\nOutput file was not found in input file: '%s'\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sInputFileName);
         bRunApp = false;
      }

      // Check parameters
      if (sPARAM_Sex == NULL) {
         WriteToFile(pErrorStream,"\n<ERROR>\nSex value(s) was not found in input file: '%s'\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sInputFileName);
         bRunApp = false;
      }
      if (sPARAM_Race == NULL) {
         WriteToFile(pErrorStream,"\n<ERROR>\nRace value(s) was not found in input file: '%s'\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sInputFileName);
         bRunApp = false;
      }
      if (sPARAM_YOB == NULL) {
         WriteToFile(pErrorStream,"\n<ERROR>\nYear of Birth value(s) was not found in input file: '%s'\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sInputFileName);
         bRunApp = false;
      }

      // Check the optional sPARAM_NumReps value if we are not using a vector
      if (sPARAM_NumReps != NULL && !bHaveVectorValues && !IsValidNumReps(sPARAM_NumReps)) {
         WriteToFile(pErrorStream, "\n<ERROR>\nInvalid Number of Repetitions: %s,\nValue must be a positive integer with a max value of %d.\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
          sPARAM_NumReps, MAX_NUM_REPS);
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
         WriteToFile(pErrorStream,"\n<ERROR>\nSupplied Output file: '%s', could not be opened for writing.\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",sOutputFile);
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
         // TODO: Review warning about parenthesis here (probably need brackets around the second part of the condition)
      	if ((wValuesPerParam[i] > wMaxNumPerParam) && 
             (wMaxNumPerParam > 1) || 
             (wMaxNumPerParam > 1 && 
              (wValuesPerParam[i] != wMaxNumPerParam && wValuesPerParam[i] > 1))) {

            bRunApp = false;
            WriteToFile(pErrorStream, "\n<ERROR>");
            WriteToFile(pErrorStream, "\nInvalid use of vector values in the input file.");
            WriteToFile(pErrorStream, "\nIf vector values are used for more than 1 variable,");
            WriteToFile(pErrorStream, "\nthe same number of values must be supplied for each variable.");
            WriteToFile(pErrorStream, "\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n");
	      } else if (wValuesPerParam[i] > wMaxNumPerParam) {
	         wMaxNumPerParam = wValuesPerParam[i];
	      }
	   } // end for
   } // end if (bRunApp && bHaveVectorValues)

   if (bRunApp) {
      if (sSEED_Init == NULL || atol(sSEED_Init) == -1)
         lSeed_Init = time(0);
      else {
         lSeed_Init = atol(sSEED_Init);
      }
      if (sSEED_Cess == NULL || atol(sSEED_Cess) == -1)
         lSeed_Cess = time(0);
      else
         lSeed_Cess = atol(sSEED_Cess);

      if (sSEED_OCD == NULL || atol(sSEED_OCD) == -1)
         lSeed_OCD = time(0);
      else
         lSeed_OCD = atol(sSEED_OCD);

      if (sSEED_Misc == NULL || atol(sSEED_Misc) == -1)
         lSeed_Misc = time(0);
      else
         lSeed_Misc = atol(sSEED_Misc);

      if (sImmediateCess == NULL) {
         wCessationYear = 0;
      } else {
         wCessationYear = (short) atoi(sImmediateCess);
      }

      try {
         short wOutputType = 1; // OUT_DataOnly=1
         pSimulator = new Smoking_Simulator(sFILE_InitProb,  sFILE_CessProb,
                                            sFILE_OCDProb,   sFILE_Quintiles,
                                            sFILE_CPDData,   wOutputType,
                                            wCessationYear);

         // You may use additional strategies of class RNG_Strategy and instantiate them here without changing the smoking_sim class
         if (strcmp(sRNGStrategy, "RngStream") == 0){
            pSimulator->setRNGStrategy(new RngStreamRNG(rngStreamSeed));
         }
         else if (strcmp(sRNGStrategy, "MersenneTwister") == 0){
            pSimulator->setRNGStrategy(new MersenneTwisterRNG(lSeed_Init, lSeed_Cess, lSeed_OCD, lSeed_Misc));
         }
         else {
            WriteToFile(pErrorStream, "\n<ERROR>\nInvalid RNG Strategy: '%s'\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n", sRNGStrategy);
            bRunApp = false;
         }
         if (!gWithHoldTags) {
            WriteRunInfoTag(pOutStream, VERSION_NUM, sSEED_Init, sSEED_Cess, sSEED_OCD,
                         sSEED_Misc, sImmediateCess, sFILE_InitProb, sFILE_CessProb, sFILE_OCDProb,
                         sFILE_Quintiles, sFILE_CPDData, sOutputFile, sErrorFile, sRNGStrategy, sFinalRngStreamSeed);
         }

      } catch (SimException ex) {
	      WriteToFile(pErrorStream, "\n<ERROR>%s</ERROR>\n", ex.GetError());
         WriteToFile(pErrorStream, "<CALLPATH>%s</CALLPATH>", ex.GetCallPath());
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
               WriteToFile(pOutStream, "<SIMULATION>\n");
            }
            WriteInputTag(pOutStream, sVecValues[0], sVecValues[1], sVecValues[2], sVecValues[3]);
            if (!gWithHoldTags) {
               WriteToFile(pOutStream, "<RUN>\n");
            }
            if (bUseNumReps && !IsValidNumReps(sVecValues[3])) {
	            WriteToFile(pErrorStream, "\n<ERROR>\nInvalid Number of Repetitions: %s, \n Value must be a positive integer with a max value of %d.\n</ERROR>", sVecValues[3], MAX_NUM_REPS);
               WriteToFile(pErrorStream, "\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>");
               WriteToFile(pOutStream, "<RESULT>\nERROR\n</RESULT>\n</RUN>\n</SIMULATION>\n");
            } else if (bUseNumReps) {
               lNumReps = atol(sVecValues[3]);
               for (j=0; j<lNumReps && bRunApp;j++) {
                  try {
                     pSimulator->RunSimulationSingle(atoi(sVecValues[0]), atoi(sVecValues[1]),
                                               atoi(sVecValues[2]), pOutStream);
                  } catch (SimException ex) {
            	      WriteToFile(pErrorStream,"\n<ERROR>%s</ERROR>\n",ex.GetError());
                     WriteToFile(pErrorStream,"<CALLPATH>%s</CALLPATH>",ex.GetCallPath());
                     bRunApp = (ex.GetType() == SimException::NON_FATAL);
                     WriteToFile(pOutStream,"<RESULT>\nERROR\n</RESULT>\n");
                  }
               }
               WriteToFile(pOutStream,"</RUN>\n</SIMULATION>\n");
            } else {
               try {
                  pSimulator->RunSimulationSingle(atoi(sVecValues[0]), atoi(sVecValues[1]),
                                            atoi(sVecValues[2]), pOutStream);
               } catch (SimException ex) {
           	      WriteToFile(pErrorStream,"\n<ERROR>%s</ERROR>\n",ex.GetError());
                  WriteToFile(pErrorStream,"<CALLPATH>%s</CALLPATH>",ex.GetCallPath());
                  bRunApp = (ex.GetType() == SimException::NON_FATAL);
                  WriteToFile(pOutStream,"<RESULT>\nERROR\n</RESULT>\n");
               }
               if (!gWithHoldTags) 
                  WriteToFile(pOutStream,"</RUN>\n</SIMULATION>\n");
            }
	      }  
      } else if (bUseNumReps) {
         if (!gWithHoldTags) 
            WriteToFile(pOutStream,"<SIMULATION>\n");
         WriteInputTag(pOutStream,sPARAM_Race,sPARAM_Sex,sPARAM_YOB,sPARAM_NumReps);
         if (!gWithHoldTags) 
            WriteToFile(pOutStream,"<RUN>\n");
         for (j=0; j<lNumReps && bRunApp; j++) {
            try {
               pSimulator->RunSimulationSingle(atoi(sPARAM_Race),atoi(sPARAM_Sex),atoi(sPARAM_YOB),pOutStream);
            } catch(SimException ex) {
        	      WriteToFile(pErrorStream,"\n<ERROR>%s</ERROR>\n",ex.GetError());
               WriteToFile(pErrorStream,"<CALLPATH>%s</CALLPATH>",ex.GetCallPath());
               bRunApp = (ex.GetType() == SimException::NON_FATAL);
               WriteToFile(pOutStream,"<RESULT>\nERROR\n</RESULT>\n");
            }
         }
         if (!gWithHoldTags) 
            WriteToFile(pOutStream,"</RUN>\n</SIMULATION>\n");
      } else {
         try {
            WriteToFile(pOutStream,"<SIMULATION>\n");
            WriteInputTag(pOutStream,sPARAM_Race,sPARAM_Sex,sPARAM_YOB,sPARAM_NumReps);
            WriteToFile(pOutStream,"<RUN>\n");
            pSimulator->RunSimulationSingle(atoi(sPARAM_Race),atoi(sPARAM_Sex),atoi(sPARAM_YOB),pOutStream);
            if (!gWithHoldTags)
               WriteToFile(pOutStream,"</RUN>\n</SIMULATION>\n");
         } catch (SimException ex) {
            WriteToFile(pErrorStream,"\n<ERROR>%s</ERROR>\n",ex.GetError());
            WriteToFile(pErrorStream,"<CALLPATH>%s</CALLPATH>",ex.GetCallPath());
            bRunApp = (ex.GetType() == SimException::NON_FATAL);
            WriteToFile(pOutStream,"<RESULT>\nERROR\n</RESULT>\n");
            WriteToFile(pOutStream,"</RUN>\n</SIMULATION>\n");
         }
      }
   }
   if (pOutStream != NULL)
      fclose(pOutStream);
   

   if (pErrorStream != NULL) {

      fclose(pErrorStream);
      ifstream errorFile(sErrorFile, ios::binary | ios::ate);
      long fileSize = errorFile.tellg();
      errorFile.close();
      if (fileSize == 0) {
         remove(sErrorFile);
      }
   }
   // JC: it seems wrong to return 1 when bRunApp==True (indicating a normal execution)
   // However, it was that way for a long time, so I'm not sure if it was intentional
   // TODO: Review this and make sure it's correct
   if (bRunApp)
      iReturnValue = 0;
   else
      iReturnValue = 1;

   delete [] sErrorFile;
   delete [] sRNGStrategy;
   delete [] sInputBuffer;
   delete [] sFILE_InitProb;
   delete [] sFILE_CessProb;
   delete [] sFILE_OCDProb;
   delete [] sFILE_Quintiles;
   delete [] sFILE_CPDData;
   free(sSEED_Init);
   free(sSEED_Cess);
   free(sSEED_OCD);
   free(sSEED_Misc);
   delete [] sOutputFile;
   delete [] sImmediateCess;
   delete [] sPARAM_Sex;
   delete [] sPARAM_Race;
   delete [] sPARAM_YOB;
   delete [] sPARAM_NumReps;
   delete pSimulator;

   return iReturnValue;
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

// Print a usage message to STDERR
void Usage(void) {
   PrintMessage("Usage:\n");
   PrintMessage(" Smoking_Initiation\n");
   PrintMessage("        Runs a user interface version of program.\n\n");
   PrintMessage("Or\n\n");
   PrintMessage(" Smoking_Initiation DATA_DIR INIT_SEED CESS_SEED OTH_COD_SEED INPUT_FILE OUTPUT_FILE OUTPUT_TYPE CESS_YEAR\n");
   PrintMessageFormatted("\nOr\n\n");
   PrintMessage(" Smoking_Initiation INIT_SEED CESS_SEED OTH_COD_SEED INPUT_FILE OUTPUT_FILE OUTPUT_TYPE CESS_YEAR\n");
   PrintMessage(" Where:\n");
   PrintMessage("    DATA_DIR     - Directory that contains the input files used by the application \n");
   PrintMessage("    INIT_SEED    - An integer seed for the Initiation Probability PRNG (>= 0)\n");
   PrintMessage("    CESS_SEED    - An integer seed for the Cessation Probability PRNG (>= 0)\n");
   PrintMessage("    OTH_COD_SEED - An integer seed for the Other Cause of Death Probability PRNG (>= 0)\n");
   PrintMessage("    INDIV_SEED   - An integer seed for the PRNG that will be used for defining characteristics of the individual(>= 0)\n");
   PrintMessage("    INPUT_FILE   - Name of file containing co-variates to use in simulation\n");
   PrintMessage("    OUTPUT_FILE  - Path where output will be written\n");
   PrintMessage("    OUTPUT_TYPE  - Format for output file (1=Data, 2=Text, 3=Timeline)\n");
   PrintMessage("    CESS_YEAR    - 4-digit Year Value. All smokers will stop smoking on January 1st of year provided.\nEnter a value of '0' to disable the immediate cessation option.\n");
   PrintMessage(" Press any key to close window");
   getc(stdin);
}


// Validate the parameters necessary to run application
bool ValidateParameters(char* sDataFileDir, 
                        char* sInitiationSeed, char* sCessationSeed, char* sOtherCODSeed, char* sIndivRndSeed, 
                        char* sInputFile, char* sOutputFile,
                        char* sOutputType, char* sImmediateCess, char* sErrorMessage) {

   bool bReturnValue = true;
   char *sTestDirStr;
	FILE *pTestInputStream  = 0;

   sTestDirStr = AssignFilename(sDataFileDir, INITIATION_DATA_FILE);
	pTestInputStream = fopen(sTestDirStr, "r");

	if (pTestInputStream == NULL) {
		snprintf(sErrorMessage, ERROR_MESSAGE_SIZE, "Input File %s could not be opened for reading.\n", sTestDirStr);
		bReturnValue = false;
  	}
	if (pTestInputStream != NULL) {
      fclose(pTestInputStream);
   }
   if (bReturnValue) {
      bReturnValue = ValidateParameters(sInitiationSeed, sCessationSeed, sOtherCODSeed, sIndivRndSeed,
                                        sInputFile, sOutputFile, 
                                        sOutputType, sImmediateCess, sErrorMessage);
   }
   return bReturnValue;
}


// Validate the parameters necessary to run application
bool ValidateParameters(char* sInitiationSeed, char* sCessationSeed, char* sOtherCODSeed, char* sIndivRndSeed,
                        char* sInputFile, char* sOutputFile,
                        char* sOutputType, char* sImmediateCess, char* sErrorMessage) {

	FILE *pTestInputStream  = 0,
		  *pTestOutputStream = 0;
	bool bReturnValue = true;

	if (!IsPosLongInt(sInitiationSeed)) {
		snprintf(sErrorMessage, ERROR_MESSAGE_SIZE, "Invalid Seed %s for Initiation Probability PRNG.\nValid Range id 0 to %ld.\n", 
         sInitiationSeed, MAX(long));
		bReturnValue = false;
  	} else if (!IsPosLongInt(sCessationSeed)) {
		snprintf(sErrorMessage, ERROR_MESSAGE_SIZE,"Invalid Seed %s for Cessation Probability PRNG.\nValid Range id 0 to %ld.\n", 
         sCessationSeed, MAX(long));
		bReturnValue = false;
  	} else if (!IsPosLongInt(sOtherCODSeed)) {
		snprintf(sErrorMessage, ERROR_MESSAGE_SIZE,"Invalid Seed %s for Other Cause of Death Probability PRNG.\nValid Range id 0 to %ld.\n", 
         sOtherCODSeed, MAX(long));
		bReturnValue = false;
  	} else if (!IsPosLongInt(sIndivRndSeed)) {
		snprintf(sErrorMessage, ERROR_MESSAGE_SIZE,"Invalid Seed %s for Indivdual's Random Numbers PRNG.\nValid Range id 0 to %ld.\n", 
         sIndivRndSeed, MAX(long));
		bReturnValue = false;
  	} else if (!IsPosShortInt(sImmediateCess) ||
              ((atoi(sImmediateCess) != 0) &&
               (atoi(sImmediateCess) < wMIN_IMMEDIATE_CESSATION_YEAR || 
                atoi(sImmediateCess) > wSIM_CUTOFF_YEAR))) {
      snprintf(sErrorMessage, ERROR_MESSAGE_SIZE, "Invalid value %s for Immediate Cessation Year. \nValid values are 0, %d-%d.\n", 
              sImmediateCess, wMIN_IMMEDIATE_CESSATION_YEAR, wSIM_CUTOFF_YEAR);
      bReturnValue = false;
   } else if (!IsPosShortInt(sOutputType) ||
           (atoi(sOutputType) < (short)Smoking_Simulator::OUT_DataOnly) ||
           (atoi(sOutputType) >= (short)Smoking_Simulator::OUT_Uninitialized)) {
      snprintf(sErrorMessage, ERROR_MESSAGE_SIZE,"Invalid Output Type: %d\nValid values are %d to ?.\n",
              (short)Smoking_Simulator::OUT_DataOnly, ((short)Smoking_Simulator::OUT_Uninitialized-1));
		bReturnValue = false;
  	}

	// Make sure input and output files can be opened for reading/writing respectively
	if (bReturnValue) {
		pTestInputStream  = fopen(sInputFile, "r");
		pTestOutputStream = fopen(sOutputFile, "w");
		if (pTestInputStream == NULL) {
			snprintf(sErrorMessage, ERROR_MESSAGE_SIZE, "Input File %s could not be opened for reading.\n", sInputFile);
			bReturnValue = false;
	  	}
      if (pTestInputStream  != NULL) {
         fclose(pTestInputStream);
      }
		if (bReturnValue && pTestOutputStream == NULL) {
         snprintf(sErrorMessage, 1000, "Output File %s could not be opened for writing.\n", sOutputFile);
         bReturnValue = false;
	  	}
		if (pTestOutputStream != NULL) {
         fclose(pTestOutputStream);
      }
  	}
	return bReturnValue;
}

//Writes out tagged information about the program to pOutStream
void WriteRunInfoTag(FILE* pOutStream, const char* sVersion, const char* sInitiationSeed,
                     const char* sCessSeed, const char* sOCDSeed, const char* sMiscSeed,
                     const char* sImmediateCessYear, const char* sInitFile, const char* sCessFile,
                     const char* sOCDProbFile, const char* sQuintilesFile, const char* sCPDDataFile,
                     const char* sOutputFile, const char* sErrorFile, const char* sRNGStrategy, 
                     const char* sRngStreamSeed) {
   if (pOutStream == NULL)
      throw SimException("WriteRunInfoTag()::ERROR","Output stream is not initialized.\n");

   WriteToFile(pOutStream,"<RUNINFO>\n");
   WriteToFile(pOutStream,"<VERSION>%s</VERSION>\n", sVersion);
   WriteToFile(pOutStream,"<RNGSTRATEGY>%s</RNGSTRATEGY>\n", sRNGStrategy);
   WriteToFile(pOutStream,"<SEEDS>\n");
   if (strcmp(sRNGStrategy, "MersenneTwister") == 0) {
      WriteToFile(pOutStream,"<INIT_PRNG_SEED>%s</INIT_PRNG_SEED>\n", sInitiationSeed);
      WriteToFile(pOutStream,"<CESS_PRNG_SEED>%s</CESS_PRNG_SEED>\n", sCessSeed);
      WriteToFile(pOutStream,"<OCD_PRNG_SEED>%s</OCD_PRNG_SEED>\n", sOCDSeed);
      WriteToFile(pOutStream,"<MISC_PRNG_SEED>%s</MISC_PRNG_SEED>\n", sMiscSeed);
   } else {
      WriteToFile(pOutStream,"<RNGSTREAM_SEED>%s</RNGSTREAM_SEED>\n", sRngStreamSeed);
   }
   WriteToFile(pOutStream,"</SEEDS>\n");
   WriteToFile(pOutStream,"<DATAFILES>\n");
   WriteToFile(pOutStream,"<INPUT_FILE>%s</INPUT_FILE>\n", gInputFileName.c_str());
   WriteToFile(pOutStream,"<INITIATION>%s</INITIATION>\n", sInitFile);
   WriteToFile(pOutStream,"<CESSATION>%s</CESSATION>\n", sCessFile);
   WriteToFile(pOutStream,"<OCD>%s<OCD>\n", sOCDProbFile);
   WriteToFile(pOutStream,"<CIG_PER_DAY>%s</CIG_PER_DAY>\n</DATAFILES>\n", sCPDDataFile);
   WriteToFile(pOutStream,"<OUTFILES>\n<OUTPUT>%s</OUTPUT>\n", sOutputFile);
   WriteToFile(pOutStream,"<ERRORS>%s</ERRORS>\n</OUTFILES>\n", sErrorFile);
   WriteToFile(pOutStream,"<OPTIONS>\n<CESSATION_YR>%s</CESSATION_YR>\n", sImmediateCessYear);
   WriteToFile(pOutStream,"</OPTIONS>\n</RUNINFO>\n");
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
         WriteToFile(pOutStream, "<INPUTS>\n");

         if (iRace >= 0 && iRace < Smoking_Simulator::NUM_RACES) {
            WriteToFile(pOutStream, "<RACE>%s</RACE>\n", sRACE_LABELS[iRace]);
         } else {
            WriteToFile(pOutStream, "<RACE>\n%d\n</RACE>\n", iRace);
         }

         if (iSex >= 0 && iSex < Smoking_Simulator::NUM_SEXES) {
            WriteToFile(pOutStream,"<SEX>%s</SEX>\n", sSEX_LABELS[iSex]);
         } else {
            WriteToFile(pOutStream,"<SEX>\n%d\n</SEX>\n", iSex);
         }

         WriteToFile(pOutStream,"<YOB>%s</YOB>\n",sYearOfBirth);
         if (sNumReps != NULL && (strcmp(sNumReps,"\0") != 0)) {
            WriteToFile(pOutStream,"<REPEAT>%s</REPEAT>\n", sNumReps);
   	   }
         WriteToFile(pOutStream,"</INPUTS>\n");
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

// Create a data file simulating  sNumToSimulate people for each race/sex/year of birth
// Assume 20 cigarettes per day for current smokers
// All seeds have the value of 0
// This routine is used during the applications development to test the results
// It SHOULD NOT be used by/with the CISNET models
bool CreateDataFile(const char *sNumToSimulate, const char* sOutFileName, char* sErrorMessage) {

   bool bReturnValue = true;
   Smoking_Simulator *pSimulator = 0;
   FILE *pOutputFile = 0;
   short i, j, k;
   long l, lNumToSimulate;

   if (IsPosLongInt(sNumToSimulate)) {
      lNumToSimulate = atol(sNumToSimulate);
      try {
         short wCessationYear = 0;
         pSimulator = new Smoking_Simulator(INITIATION_DATA_FILE, CESSATION_DATA_FILE,
                                            OTHER_COD_DATA_FILE,  CPD_INTENSITY_PROBS,
                                            CPD_DATA_FILE,        0,
                                            0,                    0,
                                            0,                    Smoking_Simulator::OUT_DataOnly,
                                            wCessationYear);

         pOutputFile = fopen(sOutFileName, "w");
         for (i = 1; i <= pSimulator->GetNumRaceValues(); i++) {
            for (j = 1; j<= pSimulator->GetNumSexValues(); j++) {
               for (k = pSimulator->GetMinYearOfBirth(); k <= pSimulator->GetMaxYearOfBirth(); k++) {
                  for (l = 0; l < lNumToSimulate; l++) {
                     pSimulator->RunSimulationSingle( i, j, k, pOutputFile);
                  }
                  PrintMessageFormatted("%d %d %d\n",i,j,k);
               }
            }
         }
         fclose(pOutputFile);

      } catch (SimException ex) {
         // * is replaced with 1000-1=999 characters to allow for the null terminator
         snprintf(sErrorMessage, ERROR_MESSAGE_SIZE, "%.*s", ERROR_MESSAGE_SIZE-1, ex.GetError());
         if (pSimulator) {
            delete pSimulator;
            pSimulator = nullptr;
         }
		   bReturnValue = false;
   	} catch (...) {
         snprintf(sErrorMessage, ERROR_MESSAGE_SIZE, "Unknown Error Occurred\n");
         delete pSimulator;
		   bReturnValue = false;
   	}
   } else {
      snprintf(sErrorMessage, ERROR_MESSAGE_SIZE, "Invalid value: %s, supplied for number of simulations to run.\n", sNumToSimulate);
      bReturnValue = false;
   }
   delete pSimulator;

	return bReturnValue;
}

string RngStreamToString(unsigned long arr[], int length) {
   string sRngStream;
   //int length = 6; // RngStream has 6 values
   for (int i = 0; i < length; i++) {
      sRngStream += to_string(arr[i]);
      if (i < length - 1) {
         sRngStream += ",";
      }
   }
   return sRngStream;
}