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
#include <thread>
#include <atomic>
#include <filesystem>
#include <chrono>
#include <memory>
#include <exception>

#include "smoking_sim.h"
#include "sim_exception.h"
#include "rng_strategy.h"
#include "version.h"

#ifdef IS_R
  #include <Rcpp.h>
#endif

// Windows-specific crash handler to prevent silent failures
// SEH exceptions (divide-by-zero, access violation) bypass C++ catch blocks
// This handler ensures we print a diagnostic message before crashing
#ifdef _WIN32
#include <windows.h>
#include <excpt.h>

static const char* GetExceptionName(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:         return "ACCESS_VIOLATION";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "INTEGER_DIVIDE_BY_ZERO";
        case EXCEPTION_INT_OVERFLOW:             return "INTEGER_OVERFLOW";
        case EXCEPTION_STACK_OVERFLOW:           return "STACK_OVERFLOW";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "FLOAT_DIVIDE_BY_ZERO";
        case EXCEPTION_FLT_OVERFLOW:             return "FLOAT_OVERFLOW";
        case EXCEPTION_FLT_UNDERFLOW:            return "FLOAT_UNDERFLOW";
        case EXCEPTION_ILLEGAL_INSTRUCTION:      return "ILLEGAL_INSTRUCTION";
        default:                                 return "UNKNOWN_EXCEPTION";
    }
}

static LONG WINAPI CrashHandler(EXCEPTION_POINTERS* pExceptionInfo) {
    DWORD code = pExceptionInfo->ExceptionRecord->ExceptionCode;
    SHG_STDERR("\n<FATAL_ERROR>\n");
    SHG_STDERR("  Windows Exception: %s (code 0x%08lX / %lu)\n",
            GetExceptionName(code), code, code);
    SHG_STDERR("  This is an unrecoverable error in the Smoking History Generator.\n");
    SHG_STDERR("  If using parallel processing, try reducing NUM_SEGMENTS or NUM_THREADS.\n");
    SHG_STDERR("  Please report this issue with your input configuration.\n");
    SHG_STDERR("</FATAL_ERROR>\n");
#ifndef IS_R
    fflush(stderr);
#endif
    return EXCEPTION_CONTINUE_SEARCH;  // Let Windows handle the crash after we've logged
}

static void InstallCrashHandler() {
    SetUnhandledExceptionFilter(CrashHandler);
}
#else
// No-op on non-Windows platforms (they have better error reporting)
static void InstallCrashHandler() {}
#endif

using namespace std;

#define MAX(x) (std::numeric_limits<x>::max())
#define DEFAULT_DATA_DIR const_cast<char*>("data/NHIS-1965-2016/")
#define COUNTERFACTUAL_DATA_DIR const_cast<char*>("data/counterfactual_inputs_jan_2009/")

// Input file names (bundled under data/NHIS-1965-2016/)
// DEFAULT_MORTALITY_DATA_FILE was historically OTHER_COD_DATA_FILE: the bundled default is
// other-cause mortality excluding lung cancer; users may point configs at all-cause (acm.txt) instead.
#define INITIATION_DATA_FILE "initiation.txt"
#define CESSATION_DATA_FILE "cessation.txt"
#define DEFAULT_MORTALITY_DATA_FILE "ocm-excl-lung-cancer.txt"
#define CPD_INTENSITY_PROBS "cpd.txt"
#define CPD_DATA_FILE "cpd.txt"

#define MT_INIT_SEED_DEFAULT "1898587603"
#define MT_CESS_SEED_DEFAULT "1468371936"
#define MT_MORTALITY_SEED_DEFAULT "1551308340"
#define MT_MISC_SEED_DEFAULT "1590227640"

const unsigned long RNGSTREAM_SEED_DEFAULT[6] = {12345, 12345, 12345, 12345, 12345, 12345};

#define VECTOR_DELIMITER ","
#define MAX_NUM_REPS 10000000
#define ERROR_MESSAGE_SIZE 1000

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
// CLI-specific wrappers for shared XML writing functions (pass globals gInputFileName, gWithHoldTags)
void WriteRunInfoTagCLI(FILE*, const char*, const char*, const char*, const char*,
                        const char*, const char*, const char*, const char*, const char*,
                        const char*, const char*, const char*, const char*, const char*, const char*,
                        int, int, bool, bool);
void WriteInputTagCLI(FILE* , char*, char*, const char*, const char*);
string RngStreamToString(unsigned long arr[], int length);

// Segment runner for parallel processing
// Optimized for cache alignment to reduce false sharing between threads
struct alignas(64) SegmentParams {
   // Hot data (accessed frequently in worker threads) - grouped together
   SmokingSimulatorSharedData* sharedData;
   RNG_Strategy* preCreatedRng;  // Pre-created RNG to avoid lock contention
   long startRep;
   long endRep;
   int segmentIndex;
   int race;
   int sex;
   int yob;
   short outputType;
   short cessationYear;
   
   // Cold data (accessed once during setup)
   char tempFilePath[256];  // Fixed size array instead of std::string for better cache locality
   unsigned long rngSeed[6];
};

// ==============================================================================
// OutputBuffer: Memory buffer for batching output writes
// Reduces I/O overhead by accumulating data in memory before flushing to disk
// ==============================================================================
class OutputBuffer {
private:
    std::vector<char> buffer;
    size_t pos;
    FILE* output_file;
    static constexpr size_t BUFFER_SIZE = 4 * 1024 * 1024;  // 4 MB buffer
    static constexpr size_t FLUSH_THRESHOLD = BUFFER_SIZE - 100000;  // Leave 100KB safety margin
    
public:
    explicit OutputBuffer(FILE* file) : output_file(file), pos(0) {
        buffer.resize(BUFFER_SIZE);
    }
    
    void append(const char* data, size_t len) {
        if (pos + len > FLUSH_THRESHOLD) {
            flush();
        }
        if (len < BUFFER_SIZE) {
            memcpy(buffer.data() + pos, data, len);
            pos += len;
        } else {
            // Very large write - flush then write directly
            flush();
            fwrite(data, 1, len, output_file);
        }
    }
    
    void flush() {
        if (pos > 0 && output_file) {
            fwrite(buffer.data(), 1, pos, output_file);
            pos = 0;
        }
    }
    
    ~OutputBuffer() {
        flush();
    }
};

void RunSegment(const SegmentParams& params);
void AssembleSegmentFiles(const std::vector<std::string>& tempFiles, const std::string& outputFile, bool withTags);

// Removing main() when IS_R is defined (R / shg-r package build); standalone CLI needs main()
// But we include all the other methods and variables to avoid DRY violations in the Rcpp wrapper
#ifdef IS_R
#else
int main(int argc, char* argv[]) {
   // Install crash handler to prevent silent failures on Windows
   InstallCrashHandler();
   
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
                      char* sCessationSeed, char* sMortalitySeed,
                      char* sIndivRndSeed, char* sInputFile,
                      char* sOutputFile, char* sOutputType,
                      char* sImmediateCess, char* sErrorMessage) {

	bool						bReturnValue = true;
   short                wOutputType,
                        wCessationYear;
	unsigned long 			ulInitiationSeed,
					  			ulCessationSeed,
                        ulMortalitySeed,
                        ulIndivRndSeed;
   char                *sInitiationFile = 0,
                       *sCessationFile = 0,
                       *sMortalityFile = 0,
                       *sCPDIntensityFile = 0,
                       *sCPDDataFile = 0;
	Smoking_Simulator	  *pSimulator  = 0;

	try {
      sInitiationFile = AssignFilename(sDataFileDir, INITIATION_DATA_FILE);
      sCessationFile = AssignFilename(sDataFileDir, CESSATION_DATA_FILE);
      sMortalityFile = AssignFilename(sDataFileDir, DEFAULT_MORTALITY_DATA_FILE);
      sCPDIntensityFile = AssignFilename(sDataFileDir, CPD_INTENSITY_PROBS);
      sCPDDataFile = AssignFilename(sDataFileDir, CPD_DATA_FILE);
      ulInitiationSeed = (unsigned long) atol(sInitiationSeed);
      ulCessationSeed = (unsigned long) atol(sCessationSeed);
      ulMortalitySeed = (unsigned long) atol(sMortalitySeed);
      ulIndivRndSeed = (unsigned long) atol(sIndivRndSeed);
      wOutputType = (short) atoi(sOutputType);
      wCessationYear = (short) atoi(sImmediateCess);

  		pSimulator = new Smoking_Simulator(sInitiationFile, sCessationFile, sMortalityFile, sCPDIntensityFile, sCPDDataFile, 
                                         ulInitiationSeed, ulCessationSeed, ulMortalitySeed, ulIndivRndSeed,  
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
   delete [] sInitiationFile; delete [] sCessationFile; delete [] sMortalityFile; delete [] sCPDIntensityFile; delete [] sCPDDataFile;
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
      #ifndef IS_R
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
                       *sMortalityFile = 0,
                       *sCPDIntensityFile = 0,
                       *sCPDDataFile = 0;
   bool                 bValidInput,
                  		bKeepRepeating;
   unsigned long  		ulInitPRNGSeed,
                  		ulCessPRNGSeed,
                        ulMortalityPRNGSeed,
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
      sMortalityFile = AssignFilename(COUNTERFACTUAL_DATA_DIR, DEFAULT_MORTALITY_DATA_FILE);
      sCPDIntensityFile = AssignFilename(COUNTERFACTUAL_DATA_DIR, CPD_INTENSITY_PROBS);
      sCPDDataFile = AssignFilename(COUNTERFACTUAL_DATA_DIR, CPD_DATA_FILE);
   } else {
      sInitiationFile = AssignFilename(DEFAULT_DATA_DIR, INITIATION_DATA_FILE);
      sCessationFile = AssignFilename(DEFAULT_DATA_DIR, CESSATION_DATA_FILE);
      sMortalityFile = AssignFilename(DEFAULT_DATA_DIR, DEFAULT_MORTALITY_DATA_FILE);
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
         ulMortalityPRNGSeed = (unsigned long) atol(sInputChar);
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
                                          sMortalityFile,     sCPDIntensityFile,
                                          sCPDDataFile,        ulInitPRNGSeed,
                                          ulCessPRNGSeed,       ulMortalityPRNGSeed,
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
               #ifndef IS_R
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
   delete [] sInitiationFile; delete [] sCessationFile; delete [] sMortalityFile; delete [] sCPDIntensityFile; delete [] sCPDDataFile;
}

// Fast integer to string - writes backwards and returns pointer to start
__attribute__((always_inline))
inline char* fast_itoa(int val, char* end) {
   bool neg = val < 0;
   if (neg) val = -val;
   *--end = ';';
   if (__builtin_expect(val == 0, 0)) { *--end = '0'; }
   else {
      while (val) { *--end = '0' + (val % 10); val /= 10; }
   }
   if (neg) *--end = '-';
   return end;
}

// Fast double to string with 2 decimal places
__attribute__((always_inline))
inline int fast_dtoa2(double val, char* buf) {
   int ival = (int)(val * 100.0 + 0.5);
   int frac = ival % 100;
   ival /= 100;
   char* p = buf;
   if (__builtin_expect(ival == 0, 0)) { *p++ = '0'; }
   else {
      char tmp[12]; int i = 0;
      while (ival) { tmp[i++] = '0' + (ival % 10); ival /= 10; }
      while (i--) *p++ = tmp[i];
   }
   *p++ = '.';
   *p++ = '0' + (frac / 10);
   *p++ = '0' + (frac % 10);
   *p++ = ';';
   return p - buf;
}

// Runs a single segment - optimized with larger write buffer
void RunSegment(const SegmentParams& params) {
   FILE* pTempFile = NULL;
   try {
      auto tSegStart = std::chrono::high_resolution_clock::now();
      
      // Open temp file (tempFilePath is now a char array for better cache locality)
      pTempFile = fopen(params.tempFilePath, "w");
      if (!pTempFile) {
         throw SimException("Error", "Could not open temp file");
      }
      
      // Use larger buffer - thread_local to avoid cache contention between threads
      // Each thread gets its own buffer, eliminating false sharing
      static thread_local char file_buffer[8 * 1024 * 1024];
      setvbuf(pTempFile, file_buffer, _IOFBF, sizeof(file_buffer));
      
      // Create simulator using shared data
      Smoking_Simulator simulator(params.sharedData, params.outputType, params.cessationYear);
      
      // Prefetch probability arrays into cache at segment start
      // This warms up the cache before first access, reducing initial cache misses
      // Use temporal locality hint 0 (don't pollute cache, we'll access soon)
      if (params.sharedData->gdInitiationProbs) {
         __builtin_prefetch(params.sharedData->gdInitiationProbs, 0, 0);
      }
      if (params.sharedData->gdCessationProbs) {
         __builtin_prefetch(params.sharedData->gdCessationProbs, 0, 0);
      }
      if (params.sharedData->gdMortalityProbs) {
         __builtin_prefetch(params.sharedData->gdMortalityProbs, 0, 0);
      }
      
      // Performance optimizations (reproducibility-safe)
      // NOTE: gbSkipOversampling disabled - affects reproducibility across different NUM_SEGMENTS
      simulator.gbSkipValidation = true;     // Skip input validation (pre-validated)
      
      // Use pre-created RNG if available (avoids lock contention during parallel init)
      // Otherwise create one (fallback for legacy code paths)
      if (params.preCreatedRng) {
         simulator.setRNGStrategy(params.preCreatedRng);
      } else {
         unsigned long seed[6];
         memcpy(seed, params.rngSeed, sizeof(seed));
         RngStreamRNG* rng = new RngStreamRNG(seed);
         for (int i = 0; i < params.segmentIndex; i++) {
            rng->incrementSubstreams();
         }
         simulator.setRNGStrategy(rng);
      }
      
      auto tAfterSetup = std::chrono::high_resolution_clock::now();
      
      long numReps = params.endRep - params.startRep;
      
      // Run simulations and write using WriteAsData (DRY - same code path as R wrapper)
      for (long j = 0; j < numReps; j++) {
         simulator.RunSimulationSingle(params.race, params.sex, params.yob, pTempFile);
      }
      
      auto tAfterSim = std::chrono::high_resolution_clock::now();
      
      if (pTempFile) {
         fclose(pTempFile);
         pTempFile = NULL;
      }
      
      auto tSegEnd = std::chrono::high_resolution_clock::now();
      
      if (params.segmentIndex == 0) {
         SHG_STDERR( "    [SEG0] Setup: %lld ms, Sim+Write: %lld ms, Close: %lld ms\n",
            std::chrono::duration_cast<std::chrono::milliseconds>(tAfterSetup-tSegStart).count(),
            std::chrono::duration_cast<std::chrono::milliseconds>(tAfterSim-tAfterSetup).count(),
            std::chrono::duration_cast<std::chrono::milliseconds>(tSegEnd-tAfterSim).count());
      }
   } catch (SimException& ex) {
      if (pTempFile) {
         fclose(pTempFile);
         pTempFile = NULL;
      }
      PrintError("Segment %d error: %s\n", params.segmentIndex, ex.GetError());
      throw;  // Re-throw to propagate error to caller
   } catch (std::exception& ex) {
      if (pTempFile) {
         fclose(pTempFile);
         pTempFile = NULL;
      }
      PrintError("Segment %d std::exception: %s\n", params.segmentIndex, ex.what());
      throw;  // Re-throw to propagate error to caller
   } catch (...) {
      if (pTempFile) {
         fclose(pTempFile);
         pTempFile = NULL;
      }
      PrintError("Segment %d unknown exception\n", params.segmentIndex);
      throw;  // Re-throw to propagate error to caller
   }
}

// Assembles text segment files by concatenating them in order
void AssembleSegmentFiles(const std::vector<std::string>& tempFiles, const std::string& outputFile, bool withTags) {
   // Use larger buffer for more efficient I/O
   constexpr size_t BUFFER_SIZE = 256 * 1024;  // 256KB buffer
   
   FILE* pOutFile = fopen(outputFile.c_str(), "a");
   if (!pOutFile) {
      PrintError("Could not open output file: %s\n", outputFile.c_str());
      return;
   }
   
   // Set large buffer on output file
   setvbuf(pOutFile, NULL, _IOFBF, BUFFER_SIZE);
   
   // Pre-allocate buffer
   std::vector<char> buffer(BUFFER_SIZE);
   
   for (const auto& tempPath : tempFiles) {
      FILE* pIn = fopen(tempPath.c_str(), "r");
      if (pIn) {
         // Set large buffer on input file too
         setvbuf(pIn, NULL, _IOFBF, BUFFER_SIZE);
         
         size_t n;
         while ((n = fread(buffer.data(), 1, BUFFER_SIZE, pIn)) > 0) {
            fwrite(buffer.data(), 1, n, pOutFile);
         }
         fclose(pIn);
         std::filesystem::remove(tempPath);
      }
   }
   fclose(pOutFile);
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
        *sFILE_MortalityProb   = 0, // Mortality probabilities file (all-cause or other-cause, depending on file)
        *sFILE_Quintiles = 0, // Datafile - Smoking Intensity Quintile Placement Probabilites
        *sFILE_CPDData   = 0, // Datafile - Smoking Intensity - Cigarettes per Day by Quintile
        *sSEED_Init      = 0, // Seed - For PRNG that generates Initiation Probabilities
        *sSEED_Cess      = 0, // Seed - For PRNG that generates Cessation Probabilities
        *sSEED_Mortality       = 0, // Seed for PRNG stream used when sampling death times from mortality inputs
        *sSEED_Misc      = 0, // Seed - For PRNG that generates miscellaneous random numbers that are needed for 1 time use for a person
        *sOutputFile     = 0,
        *sImmediateCess  = 0, // Immediate Cessation Year, 0 = do not do immediate cessation
        *sPARAM_Sex      = 0, // Run Parameter - Sex
        *sPARAM_Race     = 0, // Run Parameter - Race
        *sPARAM_YOB      = 0, // Run Parameter - Year of Birth
        *sPARAM_NumReps  = 0, // Run Parameter - Number of time to repeat current set of parameters
        *sNumSegments    = 0, // Number of segments for parallel processing
        *sNumThreads     = 0, // Number of threads: -1=auto, 1=single, N=N threads
        sVecValues[4][20];

   FILE *pInputFile   = 0,
        *pOutStream   = 0,
        *pErrorStream = 0;

   int   iCurrIndex,
         iIndexLength,
         iReturnValue,
         iStringLength,
         i,
         iNumSegments = -1,  // Default to -1 (auto-calculate when multi-threaded)
         iNumThreads = -1;  // Default -1 = auto (use hardware_concurrency(), multi-threaded)

   long  lNumReps,
         lSeed_Init,
         lSeed_Cess,
         lSeed_Mortality,
         lSeed_Misc,
         j;
   unsigned long rngStreamSeed[6];
   memcpy(rngStreamSeed, RNGSTREAM_SEED_DEFAULT, sizeof(RNGSTREAM_SEED_DEFAULT));
   char *sFinalRngStreamSeed = strdup("12345,12345,12345,12345,12345,12345");

   short wValuesPerParam[4],
         wMaxNumPerParam,
         wCessationYear;
   
   bool  bAutoSegments = false;       // Track if segments were auto-calculated
   bool  bUserSpecifiedSegments = false;  // Track if user explicitly set NUM_SEGMENTS
   bool  bRunMultiThreaded = true;    // Derived from iNumThreads != 1 (set after parsing)

   Smoking_Simulator *pSimulator = 0;

   gInputFileName = sInputFileName;
   pInputFile = fopen(sInputFileName, "r");
   
   if (pInputFile == NULL) {
      // Config input file
      snprintf(sErrorMessage, sizeof(sErrorMessage), "The specified input file '%s' could not be opened for reading.\n", sInputFileName);
      #ifdef IS_R 
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

         // Legacy keys still accepted: SEED_OCD=, SEED_LIFETABLE=; preferred: SEED_MORTALITY=
         if (strncmp(Str_toupper(sInputBuffer), "SEED_OCD=", strlen("SEED_OCD=")) == 0) {
            iIndexLength = strlen("SEED_OCD=");
            sSEED_Mortality = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputBuffer[i + iIndexLength]!= ' ') {
                  sSEED_Mortality[iCurrIndex] = sInputBuffer[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sSEED_Mortality[iCurrIndex]='\0';
         }

         if (strncmp(Str_toupper(sInputBuffer), "SEED_LIFETABLE=", strlen("SEED_LIFETABLE=")) == 0) {
            iIndexLength = strlen("SEED_LIFETABLE=");
            sSEED_Mortality = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputBuffer[i + iIndexLength]!= ' ') {
                  sSEED_Mortality[iCurrIndex] = sInputBuffer[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sSEED_Mortality[iCurrIndex]='\0';
         }

         if (strncmp(Str_toupper(sInputBuffer), "SEED_MORTALITY=", strlen("SEED_MORTALITY=")) == 0) {
            iIndexLength = strlen("SEED_MORTALITY=");
            sSEED_Mortality = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputBuffer[i + iIndexLength]!= ' ') {
                  sSEED_Mortality[iCurrIndex] = sInputBuffer[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sSEED_Mortality[iCurrIndex]='\0';
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

         // Legacy keys still accepted: OCD_PROB=, LIFETABLE_PROB=; preferred: MORTALITY_PROB=
         if (strstr(Str_toupper(sInputBuffer), "OCD_PROB=") != NULL) {
            iIndexLength = strlen("OCD_PROB=");
            sFILE_MortalityProb = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputLine[i + iIndexLength]!= ' ') {
                  sFILE_MortalityProb[iCurrIndex] = sInputLine[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sFILE_MortalityProb[iCurrIndex]='\0';
         }

         if (strstr(Str_toupper(sInputBuffer), "LIFETABLE_PROB=") != NULL) {
            iIndexLength = strlen("LIFETABLE_PROB=");
            sFILE_MortalityProb = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputLine[i + iIndexLength]!= ' ') {
                  sFILE_MortalityProb[iCurrIndex] = sInputLine[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sFILE_MortalityProb[iCurrIndex]='\0';
         }

         if (strstr(Str_toupper(sInputBuffer), "MORTALITY_PROB=") != NULL) {
            iIndexLength = strlen("MORTALITY_PROB=");
            sFILE_MortalityProb = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputLine[i + iIndexLength]!= ' ') {
                  sFILE_MortalityProb[iCurrIndex] = sInputLine[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sFILE_MortalityProb[iCurrIndex]='\0';
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
            sRngStreamSeed = new char[(iStringLength - iIndexLength) + 1];
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

         if (strstr(Str_toupper(sInputBuffer), "NUM_SEGMENTS=") != NULL) {
            iIndexLength = strlen("NUM_SEGMENTS=");
            sNumSegments = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputBuffer[i + iIndexLength]!= ' ') {
                  sNumSegments[iCurrIndex] = sInputBuffer[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sNumSegments[iCurrIndex]='\0';
            // Accept -1 as "auto" or positive integers
            if (strcmp(sNumSegments, "-1") == 0) {
               iNumSegments = -1;  // Explicit auto-calculate
               // Note: bUserSpecifiedSegments stays false for -1 (auto)
            } else if (IsPosShortInt(sNumSegments)) {
               iNumSegments = atoi(sNumSegments);
               if (iNumSegments < 1) {
                  iNumSegments = 1;
               }
               bUserSpecifiedSegments = true;
            }
         }

         if (strstr(Str_toupper(sInputBuffer), "NUM_THREADS=") != NULL) {
            iIndexLength = strlen("NUM_THREADS=");
            sNumThreads = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputBuffer[i + iIndexLength]!= ' ') {
                  sNumThreads[iCurrIndex] = sInputBuffer[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sNumThreads[iCurrIndex]='\0';
            // Accept -1 (auto), 1 (single-threaded), or N > 1 (N threads)
            if (strcmp(sNumThreads, "-1") == 0) {
               iNumThreads = -1;  // Explicit auto
            } else if (IsPosShortInt(sNumThreads)) {
               iNumThreads = atoi(sNumThreads);
               if (iNumThreads < 1) {
                  iNumThreads = -1;  // Default to auto
               }
            }
         }

         // Note: RUN_MULTI_THREADED removed in v6.5.0 - use NUM_THREADS instead
         // NUM_THREADS: -1 = auto (multi-threaded), 1 = single-threaded, N = N threads

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
          sRNGStrategy = strdup("RngStream"); //strdup(DEFAULT_RNG_STRATEGY) not allowed
      }
      
      else if (strcmp(sRNGStrategy, "MersenneTwister") == 0) {
         //PrintMessage("Using Mersenne Twister random number generator strategy.\n");
      }
      else if (strcmp(sRNGStrategy, "RngStream") == 0 ) {
         //PrintMessage("Using RngStream random number generator strategy.\n");
      }
      else if (sRNGStrategy == nullptr || strlen(sRNGStrategy) == 0) {
         sRNGStrategy = strdup("RngStream"); 
         //PrintMessage("Using default (RngStream) random number generator strategy.\n");
      }
      else {
         PrintError("The specified RNG strategy is invalid: '%s'\n", sRNGStrategy);
         bRunApp = false;
      }

      // Note: Auto-segment calculation moved to after lNumReps is known (see below)

      if (bRunApp && pErrorStream == NULL) {
         // Due to Rcpp not allowing variadic functions, we use snprintf to do substitution ***
         PrintError(sErrorMessage);
         snprintf(sErrorMessage, 1000, "Specified error file: '%s' could not be opened for writing.\n", sErrorFile);
         #ifdef IS_R 
           Rcpp::stop(sErrorMessage); // Warning in R?
         #endif
         bRunApp = false;
      }

      // Validate RNG strategy restrictions for parallel processing (after error stream is opened)
      // Multi-threaded = iNumThreads != 1 (either -1 auto or N > 1)
      bRunMultiThreaded = (iNumThreads != 1);
      
      if (bRunApp && pErrorStream != NULL) {
         if (strcmp(sRNGStrategy, "MersenneTwister") == 0) {
            if (iNumSegments > 1) {
               WriteToFile(pErrorStream, "\n<ERROR>\nMersenneTwister RNG cannot maintain IID properties with multiple segments. MersenneTwister is restricted to 1 segment. Use RngStream for multiple segments.\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n");
               bRunApp = false;
            }
            if (bRunMultiThreaded) {
               WriteToFile(pErrorStream, "\n<ERROR>\nMersenneTwister RNG cannot maintain IID properties with parallel execution. MersenneTwister is restricted to NUM_THREADS=1. Use RngStream for parallel execution.\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n");
               bRunApp = false;
            }
         }
         // Note: Auto-segment calculation happens earlier if needed
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
         }

         if (!IsValidSeed(sSEED_Cess)) {
            WriteToFile(pErrorStream,"\n<ERROR>\nInvalid Cessation Seed: '%s' found in input file: '%s'\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                  sSEED_Cess,sInputFileName);
            bRunApp = false;
         }
         if (sSEED_Mortality == NULL) {
            sSEED_Mortality = strdup(MT_MORTALITY_SEED_DEFAULT);

         }
         if (!IsValidSeed(sSEED_Mortality)) {
            WriteToFile(pErrorStream,"\n<ERROR>\nInvalid mortality seed (SEED_MORTALITY or legacy SEED_LIFETABLE / SEED_OCD): '%s' found in input file: '%s'\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                  sSEED_Mortality,sInputFileName);
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
      if (sFILE_MortalityProb == NULL) {
         WriteToFile(pErrorStream,"\n<ERROR>\nMortality probabilities file (MORTALITY_PROB or legacy LIFETABLE_PROB / OCD_PROB) was not found in input file: '%s'\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
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

      // Auto-calculate segments if RngStream, segments not set (-1), and multi-threaded
      // Smart formula considers both cores AND repeat count
      if (strcmp(sRNGStrategy, "RngStream") == 0 && iNumSegments == -1 && bRunMultiThreaded) {
         // Get actual thread count (-1 means auto = hardware_concurrency)
         int numCores = (iNumThreads == -1) ? std::thread::hardware_concurrency() : iNumThreads;
         if (numCores < 1) numCores = 1;
         
         // Get repeat count for smart segment calculation
         long repeatCount = (sPARAM_NumReps != NULL) ? atol(sPARAM_NumReps) : 1000;
         
         // Smart formula: consider both cores and workload
         const int MIN_INDIVIDUALS_PER_SEGMENT = 1000;  // Don't over-segment small runs
         const int SEGMENT_MULTIPLIER = 10;             // ~10 segments per core for load balancing
         
         int maxSegmentsFromCores = numCores * SEGMENT_MULTIPLIER;
         int maxSegmentsFromRepeat = (int)(repeatCount / MIN_INDIVIDUALS_PER_SEGMENT);
         if (maxSegmentsFromRepeat < 1) maxSegmentsFromRepeat = 1;
         
         iNumSegments = std::min(maxSegmentsFromCores, maxSegmentsFromRepeat);
         if (iNumSegments < 1) iNumSegments = 1;
         
         bAutoSegments = true;
         SHG_STDERR( "  [INFO] Auto-calculated NUM_SEGMENTS=%d (cores=%d, repeat=%ld, min_per_seg=%d)\n", 
            iNumSegments, numCores, repeatCount, MIN_INDIVIDUALS_PER_SEGMENT);
         SHG_STDERR( "  [INFO] For exact reproduction on other machines, add: NUM_SEGMENTS=%d\n", iNumSegments);
      }
      
      // If still -1 (not auto-calculated), default to 1 (single segment)
      // This happens when: MersenneTwister, or RngStream without multi-threading
      if (iNumSegments == -1) {
         iNumSegments = 1;
      }
      
      // Warn if user explicitly set low segment count for large workloads
      if (bUserSpecifiedSegments && bRunMultiThreaded && strcmp(sRNGStrategy, "RngStream") == 0) {
         long repeatCount = (sPARAM_NumReps != NULL) ? atol(sPARAM_NumReps) : 1000;
         int numCores = (iNumThreads == -1) ? std::thread::hardware_concurrency() : iNumThreads;
         if (numCores < 1) numCores = 1;
         
         // Warn if segments seem too low for the workload
         const int INDIVIDUALS_PER_SEGMENT_THRESHOLD = 100000;  // Warn if >100k per segment
         long individualsPerSegment = repeatCount / iNumSegments;
         
         if (iNumSegments == 1 && repeatCount > 10000) {
            SHG_STDERR( "  [WARNING] NUM_SEGMENTS=1 with REPEAT=%ld and NUM_THREADS=%d.\n", repeatCount, iNumThreads);
            SHG_STDERR( "            Single segment means no parallel execution. Consider NUM_SEGMENTS=-1\n");
            SHG_STDERR( "            for auto-calculation, or set NUM_SEGMENTS=%d for better performance.\n", 
               std::min(numCores * 10, (int)(repeatCount / 1000)));
         } else if (individualsPerSegment > INDIVIDUALS_PER_SEGMENT_THRESHOLD && iNumSegments < numCores) {
            SHG_STDERR( "  [WARNING] NUM_SEGMENTS=%d may be suboptimal for REPEAT=%ld on %d cores.\n", 
               iNumSegments, repeatCount, numCores);
            SHG_STDERR( "            Consider NUM_SEGMENTS=%d for better load balancing.\n",
               std::min(numCores * 10, (int)(repeatCount / 1000)));
         }
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

      // First find the maximum number of vector values
      wMaxNumPerParam = 1;
      for (i=0; i < 4; i++) {
         if (wValuesPerParam[i] > wMaxNumPerParam) {
            wMaxNumPerParam = wValuesPerParam[i];
         }
      }
      // Then verify all non-single values match the maximum
      for (i=0; i < 4; i++) {
         if (wValuesPerParam[i] > 1 && wValuesPerParam[i] != wMaxNumPerParam) {
            bRunApp = false;
            WriteToFile(pErrorStream, "\n<ERROR>");
            WriteToFile(pErrorStream, "\nInvalid use of vector values in the input file.");
            WriteToFile(pErrorStream, "\nIf vector values are used for more than 1 variable,");
            WriteToFile(pErrorStream, "\nthe same number of values must be supplied for each variable.");
            WriteToFile(pErrorStream, "\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n");
         }
      }
   }

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

      if (sSEED_Mortality == NULL || atol(sSEED_Mortality) == -1)
         lSeed_Mortality = time(0);
      else
         lSeed_Mortality = atol(sSEED_Mortality);

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
                                            sFILE_MortalityProb,   sFILE_Quintiles,
                                            sFILE_CPDData,   wOutputType,
                                            wCessationYear);

         // You may use additional strategies of class RNG_Strategy and instantiate them here without changing the smoking_sim class
         if (strcmp(sRNGStrategy, "RngStream") == 0){
            pSimulator->setRNGStrategy(new RngStreamRNG(rngStreamSeed));
         }
         else if (strcmp(sRNGStrategy, "MersenneTwister") == 0){
            pSimulator->setRNGStrategy(new MersenneTwisterRNG(lSeed_Init, lSeed_Cess, lSeed_Mortality, lSeed_Misc));
         }
         else {
            WriteToFile(pErrorStream, "\n<ERROR>\nInvalid RNG Strategy: '%s'\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n", sRNGStrategy);
            bRunApp = false;
         }
         if (!gWithHoldTags) {
            WriteRunInfoTagCLI(pOutStream, SHG_CORE_VERSION, sSEED_Init, sSEED_Cess, sSEED_Mortality,
                         sSEED_Misc, sImmediateCess, sFILE_InitProb, sFILE_CessProb, sFILE_MortalityProb,
                         sFILE_Quintiles, sFILE_CPDData, sOutputFile, sErrorFile, sRNGStrategy, sFinalRngStreamSeed,
                         iNumSegments, iNumThreads, bRunMultiThreaded, bAutoSegments);
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
            WriteInputTagCLI(pOutStream, sVecValues[0], sVecValues[1], sVecValues[2], sVecValues[3]);
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
         WriteInputTagCLI(pOutStream,sPARAM_Race,sPARAM_Sex,sPARAM_YOB,sPARAM_NumReps);
         if (!gWithHoldTags) 
            WriteToFile(pOutStream,"<RUN>\n");
         
         // Use parallel processing if iNumSegments > 1 and using RngStream
         // (Auto-calculation of segments happened earlier if needed)
         if (iNumSegments > 1 && strcmp(sRNGStrategy, "RngStream") == 0) {
            auto tStart = std::chrono::high_resolution_clock::now();
            
            // Close output stream temporarily for parallel processing
            fclose(pOutStream);
            pOutStream = NULL;
            
            // Create shared data for all segments
            // Note: Full data loading (~32ms) is faster than cohort-specific (~53ms)
            // because filtering overhead exceeds the memory bandwidth benefit for this workload
            auto t1 = std::chrono::high_resolution_clock::now();
            SmokingSimulatorSharedData* pSharedData = Smoking_Simulator::CreateSharedData(
               sFILE_InitProb, sFILE_CessProb, sFILE_MortalityProb, sFILE_CPDData);
            auto t2 = std::chrono::high_resolution_clock::now();
            SHG_STDERR( "  [TIMING] Shared data creation: %lld ms\n", 
               static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count()));
            
            // Safety check: ensure iNumSegments is valid
            if (iNumSegments < 1) {
               WriteToFile(pErrorStream, "\n<ERROR>\nInvalid NUM_SEGMENTS=%d. Must be >= 1.\n</ERROR>\n<CALLPATH>\nMain:ParallelProcessing()\n</CALLPATH>\n", iNumSegments);
               throw SimException("Invalid NUM_SEGMENTS", "Main:ParallelProcessing()");
            }
            
            // Calculate reps per segment
            long repsPerSegment = lNumReps / iNumSegments;
            long remainder = lNumReps % iNumSegments;
            
            // Create temp file paths (use filesystem path for cross-platform compatibility)
            std::vector<std::string> tempFiles;
            std::filesystem::path outputPath(sOutputFile);
            std::filesystem::path outputDir = outputPath.parent_path();
            if (outputDir.empty()) outputDir = ".";
            
            for (int seg = 0; seg < iNumSegments; seg++) {
               std::filesystem::path tempPath = outputDir / ("shg_segment_" + std::to_string(seg) + ".tmp");
               tempFiles.push_back(tempPath.string());
            }
            
            // Prepare segment parameters
            std::vector<SegmentParams> segmentParams;
            long currentStart = 0;
            for (int seg = 0; seg < iNumSegments; seg++) {
               SegmentParams params;
               params.segmentIndex = seg;
               params.startRep = currentStart;
               params.endRep = currentStart + repsPerSegment + (seg < remainder ? 1 : 0);
               params.race = atoi(sPARAM_Race);
               params.sex = atoi(sPARAM_Sex);
               params.yob = atoi(sPARAM_YOB);
               strncpy(params.tempFilePath, tempFiles[seg].c_str(), sizeof(params.tempFilePath) - 1);
               params.tempFilePath[sizeof(params.tempFilePath) - 1] = '\0';  // Ensure null termination
               params.sharedData = pSharedData;
               memcpy(params.rngSeed, rngStreamSeed, sizeof(params.rngSeed));
               params.outputType = 1; // OUT_DataOnly
               params.cessationYear = wCessationYear;
               params.preCreatedRng = nullptr;  // Will be set below for RngStream
               
               segmentParams.push_back(params);
               currentStart = params.endRep;
            }
            
            // Pre-create RNG objects with buffering (performance optimization)
            // This eliminates mutex contention AND reduces function call overhead
            auto tRngStart = std::chrono::high_resolution_clock::now();
            if (strcmp(sRNGStrategy, "RngStream") == 0) {
               for (int seg = 0; seg < iNumSegments; seg++) {
                  // Create base RNG with proper substream
                  RngStreamRNG* rng = new RngStreamRNG(rngStreamSeed);
                  for (int i = 0; i < seg; i++) {
                     rng->incrementSubstreams();
                  }
                  
                  // Wrap with buffering (10000 values per buffer - reduces refill overhead)
                  // This maintains exact sequence but batches generation calls
                  // Larger buffer = fewer refills, better cache utilization
                  BufferedRngStreamRNG* bufferedRng = new BufferedRngStreamRNG(rng, 10000, true);
                  segmentParams[seg].preCreatedRng = bufferedRng;
               }
            }
            auto tRngEnd = std::chrono::high_resolution_clock::now();
            SHG_STDERR( "  [TIMING] RNG pre-creation: %lld ms\n",
               static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(tRngEnd-tRngStart).count()));
            
            // Run segments (parallel or sequential)
            auto t3 = std::chrono::high_resolution_clock::now();
            if (bRunMultiThreaded) {
               // Determine number of threads to use (-1 = auto)
               int availableCores = std::thread::hardware_concurrency();
               if (availableCores < 1) availableCores = 1;
               
               int maxThreads;
               if (iNumThreads == -1) {
                  // Auto mode: use all available cores
                  maxThreads = availableCores;
               } else if (iNumThreads > availableCores) {
                  // User requested more threads than available cores - cap and warn
                  SHG_STDERR( "  [WARNING] NUM_THREADS=%d exceeds available cores (%d). Using %d threads.\n",
                     iNumThreads, availableCores, availableCores);
                  SHG_STDERR( "            Using more threads than cores provides no benefit and may cause instability.\n");
                  maxThreads = availableCores;
               } else {
                  maxThreads = iNumThreads;
               }
               
               if (maxThreads < 1) maxThreads = 1;
               // Don't create more threads than segments
               if (maxThreads > iNumSegments) maxThreads = iNumSegments;
               
               SHG_STDERR( "  [INFO] Running %d segments on %d threads\n", iNumSegments, maxThreads);
               
               // Use a thread pool pattern to limit concurrent threads
               // This prevents resource exhaustion with many segments
               std::vector<std::exception_ptr> exceptions(segmentParams.size());
               std::atomic<bool> hasError(false);
               std::atomic<size_t> nextSegment(0);
               
               // Per-thread timing
               std::vector<std::chrono::high_resolution_clock::time_point> threadStartTimes(maxThreads);
               std::vector<std::chrono::high_resolution_clock::time_point> threadEndTimes(maxThreads);
               std::vector<int> segmentsPerThread(maxThreads, 0);
               
               // Worker function that processes segments from the queue
               // Uses relaxed memory ordering for better performance on the hot path
               auto worker = [&](int threadId) {
                  threadStartTimes[threadId] = std::chrono::high_resolution_clock::now();
                  while (true) {
                     // Atomically get the next segment to process
                     // Use memory_order_relaxed for better performance (order doesn't matter here)
                     size_t segIdx = nextSegment.fetch_add(1, std::memory_order_relaxed);
                     if (segIdx >= segmentParams.size()) {
                        break;  // No more segments
                     }
                     segmentsPerThread[threadId]++;
                     try {
                        RunSegment(segmentParams[segIdx]);
                     } catch (...) {
                        exceptions[segIdx] = std::current_exception();
                        hasError.store(true, std::memory_order_relaxed);
                     }
                  }
                  threadEndTimes[threadId] = std::chrono::high_resolution_clock::now();
               };
               
               // Launch worker threads (limited to maxThreads)
               std::vector<std::thread> threads;
               for (int t = 0; t < maxThreads; t++) {
                  threads.emplace_back(worker, t);
               }
               
               // Wait for all threads to complete
               for (auto& t : threads) {
                  if (t.joinable()) {
                     t.join();
                  }
               }
               
               // Print thread timing summary
               for (int t = 0; t < maxThreads; t++) {
                  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                     threadEndTimes[t] - threadStartTimes[t]).count();
                  if (t == 0) {
                     SHG_STDERR( "    [Thread 0] %d segments in %lld ms\n", 
                        segmentsPerThread[t], static_cast<long long>(duration));
                  }
               }
               
               // Check for exceptions and collect error messages
               std::vector<std::string> segmentErrors;
               if (hasError) {
                  for (size_t i = 0; i < exceptions.size(); i++) {
                     if (exceptions[i]) {
                        try {
                           std::rethrow_exception(exceptions[i]);
                        } catch (SimException& ex) {
                           segmentErrors.push_back("Segment " + std::to_string(i) + ": " + std::string(ex.GetError()));
                        } catch (std::exception& ex) {
                           segmentErrors.push_back("Segment " + std::to_string(i) + ": " + std::string(ex.what()));
                        } catch (...) {
                           segmentErrors.push_back("Segment " + std::to_string(i) + ": Unknown exception");
                        }
                     }
                  }
               }
               
               // If any segment failed, report and fail
               if (!segmentErrors.empty()) {
                  std::string errorMsg = "Parallel execution failed:\n";
                  for (const auto& err : segmentErrors) {
                     errorMsg += "  " + err + "\n";
                  }
                  WriteToFile(pErrorStream, "\n<ERROR>\n%s</ERROR>\n<CALLPATH>\nMain:ParallelProcessing()\n</CALLPATH>\n", errorMsg.c_str());
                  throw SimException(errorMsg.c_str(), "Main:ParallelProcessing()");
               }
            } else {
               // Run segments sequentially
               for (const auto& params : segmentParams) {
                  RunSegment(params);
               }
            }
            auto t4 = std::chrono::high_resolution_clock::now();
            SHG_STDERR( "  [TIMING] Segment execution: %lld ms\n",
               static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(t4-t3).count()));
            
            // Reopen output file and assemble results
            auto t5 = std::chrono::high_resolution_clock::now();
            pOutStream = fopen(sOutputFile, "a");
            AssembleSegmentFiles(tempFiles, sOutputFile, !gWithHoldTags);
            auto t6 = std::chrono::high_resolution_clock::now();
            SHG_STDERR( "  [TIMING] File assembly: %lld ms\n",
               static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(t6-t5).count()));
            
            // Release shared data
            pSharedData->release();
            
            auto tEnd = std::chrono::high_resolution_clock::now();
            SHG_STDERR( "  [TIMING] Total parallel section: %lld ms\n",
               static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(tEnd-tStart).count()));
            
         } else {
            // Original single-segment code path
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
         }
         if (!gWithHoldTags) 
            WriteToFile(pOutStream,"</RUN>\n</SIMULATION>\n");
      } else {
         try {
            WriteToFile(pOutStream,"<SIMULATION>\n");
            WriteInputTagCLI(pOutStream,sPARAM_Race,sPARAM_Sex,sPARAM_YOB,sPARAM_NumReps);
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
   delete [] sFILE_MortalityProb;
   delete [] sFILE_Quintiles;
   delete [] sFILE_CPDData;
   free(sSEED_Init);
   free(sSEED_Cess);
   free(sSEED_Mortality);
   free(sSEED_Misc);
   delete [] sOutputFile;
   delete [] sImmediateCess;
   delete [] sPARAM_Sex;
   delete [] sPARAM_Race;
   delete [] sPARAM_YOB;
   delete [] sPARAM_NumReps;
   if (sNumSegments != NULL) delete [] sNumSegments;
   if (sNumThreads != NULL) delete [] sNumThreads;
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
   PrintMessage(" Smoking_Initiation DATA_DIR INIT_SEED CESS_SEED MORTALITY_SEED INPUT_FILE OUTPUT_FILE OUTPUT_TYPE CESS_YEAR\n");
   PrintMessageFormatted("\nOr\n\n");
   PrintMessage(" Smoking_Initiation INIT_SEED CESS_SEED MORTALITY_SEED INPUT_FILE OUTPUT_FILE OUTPUT_TYPE CESS_YEAR\n");
   PrintMessage(" Where:\n");
   PrintMessage("    DATA_DIR     - Directory that contains the input files used by the application \n");
   PrintMessage("    INIT_SEED    - An integer seed for the Initiation Probability PRNG (>= 0)\n");
   PrintMessage("    CESS_SEED    - An integer seed for the Cessation Probability PRNG (>= 0)\n");
   PrintMessage("    MORTALITY_SEED - An integer seed for the mortality PRNG (>= 0); config files use SEED_MORTALITY (legacy: SEED_LIFETABLE / SEED_OCD / OTH_COD_SEED)\n");
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
                        char* sInitiationSeed, char* sCessationSeed, char* sMortalitySeed, char* sIndivRndSeed, 
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
      bReturnValue = ValidateParameters(sInitiationSeed, sCessationSeed, sMortalitySeed, sIndivRndSeed,
                                        sInputFile, sOutputFile, 
                                        sOutputType, sImmediateCess, sErrorMessage);
   }
   return bReturnValue;
}


// Validate the parameters necessary to run application
bool ValidateParameters(char* sInitiationSeed, char* sCessationSeed, char* sMortalitySeed, char* sIndivRndSeed,
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
  	} else if (!IsPosLongInt(sMortalitySeed)) {
		snprintf(sErrorMessage, ERROR_MESSAGE_SIZE,"Invalid Seed %s for mortality probability PRNG.\nValid Range id 0 to %ld.\n", 
         sMortalitySeed, MAX(long));
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

// WriteRunInfoTag and WriteInputTag are now in smoking_sim.cpp (shared with R wrapper)
// CLI-specific wrapper that passes the global gInputFileName
void WriteRunInfoTagCLI(FILE* pOutStream, const char* sVersion, const char* sInitiationSeed,
                        const char* sCessSeed, const char* sMortalitySeed, const char* sMiscSeed,
                        const char* sImmediateCessYear, const char* sInitFile, const char* sCessFile,
                        const char* sMortalityProbFile, const char* sQuintilesFile, const char* sCPDDataFile,
                        const char* sOutputFile, const char* sErrorFile, const char* sRNGStrategy, 
                        const char* sRngStreamSeed,
                        int numSegments, int numThreads, bool multiThreaded, bool autoSegments) {
   WriteRunInfoTag(pOutStream, sVersion, sInitiationSeed, sCessSeed, sMortalitySeed, sMiscSeed,
                   sImmediateCessYear, sInitFile, sCessFile, sMortalityProbFile, sQuintilesFile, 
                   sCPDDataFile, sOutputFile, sErrorFile, sRNGStrategy, sRngStreamSeed,
                   gInputFileName.c_str(), numSegments, numThreads, multiThreaded, autoSegments);
}

// CLI-specific wrapper that uses gWithHoldTags
void WriteInputTagCLI(FILE* pOutStream, char* sRace, char* sSex, const char* sYearOfBirth, const char* sNumReps) {
   WriteInputTag(pOutStream, sRace, sSex, sYearOfBirth, sNumReps, gWithHoldTags);
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
                                            DEFAULT_MORTALITY_DATA_FILE,  CPD_INTENSITY_PROBS,
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